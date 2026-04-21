#include "aegis/installer/installer.hpp"

#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <optional>
#include <unistd.h>

#include "aegis/io/cpio.hpp"
#include "aegis/crypto/crypto.hpp"
#include "aegis/io/io.hpp"
#include "aegis/common/logger.hpp"
#include "aegis/installer/manifest.hpp"
#include "aegis/crypto/payload.hpp"

namespace aegis {

PackageInstaller::PackageInstaller(const InstallOptions &options) : options_(options) {}

int PackageInstaller::install(OtaStateMachine& machine) {
    (void)::signal(SIGPIPE, SIG_IGN);

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
    setStreamLogging(options_.verbose);
    StreamReader reader(image_fd.get(), tee_fd.get());
    logStream("starting streaming install from " +
               std::string(options_.image_path == "-" ? "stdin" : options_.image_path));
    logStream("outer SWU cpio is read sequentially; no full SWU extraction is performed");

    const CpioEntry swdesc = read_cpio_entry(reader);
    if (swdesc.name != "sw-description") { fail_runtime("sw-description must be the first cpio entry"); }
    logStream("read cpio entry '" + swdesc.name + "' size=" + std::to_string(swdesc.size));

    std::uint32_t swdesc_checksum = 0;
    const std::string sw_description = reader.read_string(swdesc.size, &swdesc_checksum);
    if (swdesc_checksum != swdesc.checksum) { fail_runtime("cpio checksum mismatch for sw-description"); }
    skip_padding(reader, swdesc.size);

    CpioEntry next = read_cpio_entry(reader);
    logStream("read cpio entry '" + next.name + "' size=" + std::to_string(next.size));
    if (next.name == "sw-description.sig") {
        std::uint32_t sig_checksum = 0;
        const std::string signature = reader.read_string(next.size, &sig_checksum);
        if (sig_checksum != next.checksum) { fail_runtime("cpio checksum mismatch for sw-description.sig"); }
        skip_padding(reader, next.size);
        verify_signature(sw_description, signature, options_.config.public_key);
        logStream("verified signed sw-description successfully");
        next = read_cpio_entry(reader);
        logStream("read next cpio entry '" + next.name + "' size=" + std::to_string(next.size));
    } else {
        fail_runtime("signed sw-description is required");
    }

    std::vector<ManifestEntry> entries = parse_manifest(sw_description, options_.target_slot);
    logStream("parsed sw-description, applicable images=" + std::to_string(entries.size()) +
               (options_.target_slot.empty() ? "" : " slot=" + options_.target_slot));
    const std::optional<AesMaterial> aes =
        options_.config.aes_key.empty() ? std::optional<AesMaterial>{}
                                 : std::optional<AesMaterial>{parse_aes_key_file(options_.config.aes_key)};

    machine.setProgress(OtaState::Install, "install", 30, "Installing payload");
    for (;;) {
        if (next.name == kTrailerName) { logStream("reached cpio trailer"); break; }

        ManifestEntry *manifest = find_manifest_entry(entries, next.name);
        if (!manifest) {
            logStream("skipping unreferenced cpio entry '" + next.name + "'");
            std::uint32_t checksum = 0;
            reader.skip(next.size, &checksum);
            if (checksum != next.checksum) { fail_runtime("cpio checksum mismatch for skipped entry " + next.name); }
            skip_padding(reader, next.size);
        } else {
            logStream("dispatching cpio entry '" + next.name + "' to handler type='" + manifest->type + "'");
            IHandler &handler = (manifest->type == "raw")
                ? static_cast<IHandler &>(raw_handler_)
                : static_cast<IHandler &>(archive_handler_);
            handler.install(reader, next, *manifest, aes ? &*aes : nullptr);
            manifest->installed = true;
            logStream("handler finished for '" + next.name + "'");
        }

        next = read_cpio_entry(reader);
        logStream("read next cpio entry '" + next.name + "' size=" + std::to_string(next.size));
    }

    machine.setProgress(OtaState::Install, "install", 95, "Installation complete, finalizing");
    for (const auto &entry : entries) {
        if (!entry.installed) { fail_runtime("required image missing from swu: " + entry.filename); }
    }

    logStream("streaming install completed successfully");
    return EXIT_SUCCESS;
}

}  // namespace aegis
