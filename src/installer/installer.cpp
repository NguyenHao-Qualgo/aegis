#include "aegis/installer/installer.hpp"

#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <unistd.h>

#include "aegis/common/logging.hpp"
#include "aegis/installer/install_context.hpp"
#include "aegis/installer/archive_handler.hpp"
#include "aegis/installer/file_handler.hpp"
#include "aegis/installer/install_signal_scope.hpp"
#include "aegis/installer/manifest.hpp"
#include "aegis/installer/raw_handler.hpp"
#include "aegis/common/cpio.hpp"
#include "aegis/common/io.hpp"
#include "aegis/common/crypto.hpp"

namespace aegis {
namespace {
std::optional<std::uint64_t> get_package_size(int fd) {
    struct stat st {};

    if (::fstat(fd, &st) != 0) {
        return std::nullopt;
    }

    if (!S_ISREG(st.st_mode)) {
        return std::nullopt;
    }

    if (st.st_size < 0) {
        return std::nullopt;
    }

    return static_cast<std::uint64_t>(st.st_size);
}
}

PackageInstaller::HandlerMap PackageInstaller::createHandlers() {
    HandlerMap handlers;
    handlers.emplace("raw", std::make_unique<RawHandler>());
    handlers.emplace("archive", std::make_unique<ArchiveHandler>());
    handlers.emplace("file", std::make_unique<FileHandler>());
    return handlers;
}

PackageInstaller::PackageInstaller(const InstallOptions &options)
    : options_(options),
      handlers_(createHandlers()) {}

PackageInstaller::~PackageInstaller() = default;

IHandler& PackageInstaller::handlerFor(const std::string& type) const {
    const auto it = handlers_.find(type);
    if (it == handlers_.end() || it->second == nullptr) {
        fail_runtime("unsupported installer handler type: " + type);
    }
    return *it->second;
}

int PackageInstaller::install(OtaStateMachine& machine, std::stop_token stop) {
    ScopedInstallSignalHandlers signal_scope;
    const InstallContext ctx{machine, stop, ScopedInstallSignalHandlers::cancel_signal_flag()};

    ctx.check_cancel();

    if (options_.config.public_key.empty()) {
        fail_runtime("public key is not configured");
    }

    FileDescriptor image_fd;

    if (options_.image_path == "-") {
        image_fd.reset(::dup(STDIN_FILENO));
    } else {
        image_fd.reset(::open(options_.image_path.c_str(), O_RDONLY));
    }

    if (!image_fd) {
        fail_runtime("cannot open input image");
    }

    const std::optional<std::uint64_t> package_size = get_package_size(image_fd.get());

    auto& progress = machine.progress();

    progress.complete(ProgressPhase::InstallVerify);

    ByteProgressTracker install_progress(
        progress,
        ProgressPhase::InstallPayload,
        package_size
    );

    FileDescriptor tee_fd;

    StreamReader reader(
        image_fd.get(),
        tee_fd.get(),
        [&install_progress](std::size_t bytes_read) {
            install_progress.add(bytes_read);
        }
    );

    LOG_I("starting streaming install from {}",
          options_.image_path == "-" ? "stdin" : options_.image_path);

    LOG_I("outer SWU cpio is read sequentially; no full SWU extraction is performed");

    if (package_size) {
        LOG_I("package size={} bytes", *package_size);
    } else {
        LOG_I("package size is unknown; byte-based progress fallback will be limited");
    }

    ctx.check_cancel();

    const CpioEntry swdesc = read_cpio_entry(reader);

    if (swdesc.name != "sw-description") {
        fail_runtime("sw-description must be the first cpio entry");
    }

    LOG_I("read cpio entry '{}' size={}", swdesc.name, swdesc.size);

    std::uint32_t swdesc_checksum = 0;
    const std::string sw_description = reader.read_string(swdesc.size, &swdesc_checksum);

    if (swdesc_checksum != swdesc.checksum) {
        fail_runtime("cpio checksum mismatch for sw-description");
    }

    skip_padding(reader, swdesc.size);

    CpioEntry next = read_cpio_entry(reader);

    LOG_I("read cpio entry '{}' size={}", next.name, next.size);

    if (next.name != "sw-description.sig") {
        fail_runtime("signed sw-description is required");
    }

    std::uint32_t sig_checksum = 0;
    const std::string signature = reader.read_string(next.size, &sig_checksum);

    if (sig_checksum != next.checksum) {
        fail_runtime("cpio checksum mismatch for sw-description.sig");
    }

    skip_padding(reader, next.size);

    verify_signature(sw_description, signature, options_.config.public_key);

    LOG_I("verified signed sw-description successfully");

    next = read_cpio_entry(reader);

    LOG_I("read next cpio entry '{}' size={}", next.name, next.size);

    ctx.check_cancel();

    auto [entries, hw_compatibility] = parse_manifest(sw_description, options_.target_slot);

    if (!hw_compatibility.empty() && hw_compatibility != options_.config.hw_compatibility) {
        fail_runtime("hardware compatibility mismatch: SWU requires '" + hw_compatibility +
                     "' but device is '" + options_.config.hw_compatibility + "'");
    }

    LOG_I("parsed sw-description, applicable images={}{}",
          std::to_string(entries.size()),
          options_.target_slot.empty() ? "" : " slot=" + options_.target_slot);

    const std::optional<AesMaterial> aes =
        options_.config.aes_key.empty()
            ? std::optional<AesMaterial>{}
            : std::optional<AesMaterial>{parse_aes_key_file(options_.config.aes_key)};

    install_progress.begin(package_size ? "Installing payload"
                                    : "Installing payload: package size unknown");

    for (;;) {
        ctx.check_cancel();

        if (next.name == kTrailerName) {
            LOG_I("reached cpio trailer");
            break;
        }

        ManifestEntry *manifest = find_manifest_entry(entries, next.name);

        if (manifest == nullptr) {
            LOG_I("skipping unreferenced cpio entry '{}'", next.name);

            std::uint32_t checksum = 0;
            reader.skip(next.size, &checksum);

            if (checksum != next.checksum) {
                fail_runtime("cpio checksum mismatch for skipped entry " + next.name);
            }

            skip_padding(reader, next.size);
        } else {
            LOG_I("dispatching cpio entry '{}' to handler type='{}'",
                  next.name,
                  manifest->type);

            IHandler& handler = handlerFor(manifest->type);

            handler.install(ctx, reader, next, *manifest, aes ? &*aes : nullptr);

            manifest->installed = true;

            LOG_I("handler finished for '{}'", next.name);
        }

        next = read_cpio_entry(reader);

        LOG_I("read next cpio entry '{}' size={}", next.name, next.size);
    }

    ctx.check_cancel();

    progress.complete(ProgressPhase::InstallFinalize);

    for (const auto& entry : entries) {
        if (!entry.installed) {
            fail_runtime("required image missing from swu: " + entry.filename);
        }
    }

    LOG_I("streaming install completed successfully");

    return EXIT_SUCCESS;
}

}  // namespace aegis
