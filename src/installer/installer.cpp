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
    machine.setProgress(OtaState::Install, "verify", 15, "Verifying package signature");
    if (options_.config.public_key.empty()) { fail_runtime("public key is not configured"); }

    FileDescriptor image_fd;
    if (options_.image_path == "-") {
        image_fd.reset(::dup(STDIN_FILENO));
    } else {
        image_fd.reset(::open(options_.image_path.c_str(), O_RDONLY));
    }
    if (!image_fd) { fail_runtime("cannot open input image"); }

    FileDescriptor tee_fd;
    StreamReader reader(image_fd.get(), tee_fd.get());
    LOG_I("starting streaming install from " +
               std::string(options_.image_path == "-" ? "stdin" : options_.image_path));
    LOG_I("outer SWU cpio is read sequentially; no full SWU extraction is performed");

    ctx.check_cancel();
    const CpioEntry swdesc = read_cpio_entry(reader);
    if (swdesc.name != "sw-description") { fail_runtime("sw-description must be the first cpio entry"); }
    LOG_I("read cpio entry '" + swdesc.name + "' size=" + std::to_string(swdesc.size));

    std::uint32_t swdesc_checksum = 0;
    const std::string sw_description = reader.read_string(swdesc.size, &swdesc_checksum);
    if (swdesc_checksum != swdesc.checksum) { fail_runtime("cpio checksum mismatch for sw-description"); }
    skip_padding(reader, swdesc.size);

    CpioEntry next = read_cpio_entry(reader);
    LOG_I("read cpio entry '" + next.name + "' size=" + std::to_string(next.size));
    if (next.name == "sw-description.sig") {
        std::uint32_t sig_checksum = 0;
        const std::string signature = reader.read_string(next.size, &sig_checksum);
        if (sig_checksum != next.checksum) { fail_runtime("cpio checksum mismatch for sw-description.sig"); }
        skip_padding(reader, next.size);
        verify_signature(sw_description, signature, options_.config.public_key);
        LOG_I("verified signed sw-description successfully");
        next = read_cpio_entry(reader);
        LOG_I("read next cpio entry '" + next.name + "' size=" + std::to_string(next.size));
    } else {
        fail_runtime("signed sw-description is required");
    }

    ctx.check_cancel();
    std::vector<ManifestEntry> entries = parse_manifest(sw_description, options_.target_slot);
    LOG_I("parsed sw-description, applicable images=" + std::to_string(entries.size()) +
               (options_.target_slot.empty() ? "" : " slot=" + options_.target_slot));
    const std::optional<AesMaterial> aes =
        options_.config.aes_key.empty() ? std::optional<AesMaterial>{}
                                 : std::optional<AesMaterial>{parse_aes_key_file(options_.config.aes_key)};

    machine.setProgress(OtaState::Install, "install", 30, "Installing payload");
    for (;;) {
        ctx.check_cancel();
        if (next.name == kTrailerName) { LOG_I("reached cpio trailer"); break; }

        ManifestEntry *manifest = find_manifest_entry(entries, next.name);
        if (!manifest) {
            LOG_I("skipping unreferenced cpio entry '" + next.name + "'");
            std::uint32_t checksum = 0;
            reader.skip(next.size, &checksum);
            if (checksum != next.checksum) { fail_runtime("cpio checksum mismatch for skipped entry " + next.name); }
            skip_padding(reader, next.size);
        } else {
            LOG_I("dispatching cpio entry '" + next.name + "' to handler type='" + manifest->type + "'");
            IHandler& handler = handlerFor(manifest->type);
            handler.install(ctx, reader, next, *manifest, aes ? &*aes : nullptr);
            manifest->installed = true;
            LOG_I("handler finished for '" + next.name + "'");
        }

        next = read_cpio_entry(reader);
        LOG_I("read next cpio entry '" + next.name + "' size=" + std::to_string(next.size));
    }

    ctx.check_cancel();
    machine.setProgress(OtaState::Install, "install", 95, "Installation complete, finalizing");
    for (const auto &entry : entries) {
        if (!entry.installed) { fail_runtime("required image missing from swu: " + entry.filename); }
    }

    LOG_I("streaming install completed successfully");
    return EXIT_SUCCESS;
}

}  // namespace aegis
