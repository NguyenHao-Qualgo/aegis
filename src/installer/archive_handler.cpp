#include "aegis/installer/archive_handler.hpp"

#include <fcntl.h>
#include <filesystem>
#include <limits.h>
#include <thread>
#include <unistd.h>

#include "aegis/common/error.hpp"
#include "aegis/common/logging.hpp"
#include "aegis/common/util.hpp"
#include "aegis/core/types.hpp"
#include "aegis/installer/archive_support.hpp"
#include "aegis/installer/handler_utils.hpp"
#include "aegis/installer/install_context.hpp"
#include "aegis/common/io.hpp"

namespace aegis {

namespace {

constexpr char kFifoName[] = "archivfifo";

fs::path resolve_archive_target(const ManifestEntry& entry, const fs::path& mountpoint) {
    if (!mountpoint.empty()) {
        return make_target_extract_path(mountpoint, entry.path);
    }
    return fs::path(entry.path);
}

void ensure_destination_directory(const ManifestEntry& entry, const fs::path& target) {
    std::error_code ec;
    if (entry.create_destination || !fs::exists(target)) {
        fs::create_directories(target, ec);
        if (ec) {
            fail_runtime("cannot create destination " + target.string());
        }
    }
}

void extract_archive_payload(const InstallContext& ctx,
                             StreamReader& reader,
                             const CpioEntry& cpio_entry,
                             const ManifestEntry& entry,
                             const AesMaterial* aes,
                             const fs::path& target) {
    char pwd[PATH_MAX] = "\0";
    if (::getcwd(pwd, sizeof(pwd)) == nullptr) {
        fail_runtime("failed to determine current working directory");
    }
    RestoreCwd restore{pwd, true};

    if (::chdir(target.c_str()) != 0) {
        fail_runtime("chdir failed for " + target.string());
    }

    fs::path fifo = fs::temp_directory_path() /
                    (std::string(kFifoName) + "-" + std::to_string(::getpid()));
    ::unlink(fifo.c_str());
    if (::mkfifo(fifo.c_str(), 0600) != 0) {
        fail_runtime("FIFO cannot be created in archive handler");
    }
    UnlinkPath fifo_guard{fifo.string()};
    LOG_I("archive handler: created FIFO '" + fifo.string() + "' for streamed archive extraction");

    ExtractData extract_data{
        .flags = archive_extract_flags(entry.preserve_attributes),
        .fifo_path = fifo.string(),
    };

    std::thread extractor([&]() { extract_archive_to_disk(&extract_data); });
    JoinThread extractor_guard{&extractor};
    LOG_I("archive handler: started extraction thread");

    FileDescriptor fdout(::open(fifo.c_str(), O_WRONLY));
    if (!fdout) {
        fail_runtime("failed to open FIFO " + fifo.string());
    }

    PayloadStreamer streamer(ctx);

    LOG_I("archive handler: writing payload bytes from SWU stream into FIFO");
    stream_payload_to_fd(streamer,
                         reader,
                         cpio_entry,
                         entry,
                         aes,
                         ctx,
                         fdout.get(),
                         "write failed: archive extractor closed the FIFO");

    fdout.reset();
    if (extractor.joinable()) {
        extractor.join();
    }

    ctx.check_cancel();
    if (extract_data.exitval != 0) {
        std::string message = "archive extraction failed for " + entry.filename;
        if (!extract_data.error_detail.empty()) {
            message += "\n" + extract_data.error_detail;
        }
        fail_runtime(message);
    }
}
}  // namespace

void ArchiveHandler::install(const InstallContext& ctx,
                             StreamReader &reader,
                             const CpioEntry &cpio_entry,
                             const ManifestEntry &entry,
                             const AesMaterial *aes) {
    ctx.check_cancel();

    if (entry.path.empty()) {
        fail_runtime("archive handler missing path for " + entry.filename);
    }

    const bool has_device = !entry.device.empty();
    const bool has_filesystem = !entry.filesystem.empty();
    if (has_device != has_filesystem) {
        fail_runtime("archive handler requires both device and filesystem when installing an image: " +
                     entry.filename);
    }

    if (has_device) {
        if (!hasFilesystemType(entry.device, entry.filesystem)) {
            fail_runtime("archive handler unsupported filesystem for " + entry.device);
        }

        makeExt4Filesystem(entry.device, true);
        ctx.check_cancel();

        const fs::path mountpoint =
            fs::temp_directory_path() / ("aegis-mnt-" + std::to_string(::getpid()));
        std::error_code ec;
        fs::create_directories(mountpoint, ec);
        if (ec) {
            fail_runtime("cannot create mountpoint " + mountpoint.string());
        }
        RemoveTree mountpoint_guard{mountpoint};

        LOG_I("archive handler: mounting device='" + entry.device + "' filesystem='" +
              entry.filesystem + "' at '" + mountpoint.string() + "'");

        mount_target_device(entry, mountpoint);
        ScopedMount scoped_mount{mountpoint, true};

        const fs::path target = resolve_archive_target(entry, mountpoint);
        LOG_I("archive handler: target path='" + target.string() + "'");
        ensure_destination_directory(entry, target);
        extract_archive_payload(ctx, reader, cpio_entry, entry, aes, target);
        LOG_I("archive handler: completed streamed extraction into '" + target.string() + "'");
        return;
    }

    const fs::path target = entry.path;
    LOG_I("archive handler: extracting into existing path '" + target.string() + "'");
    ensure_destination_directory(entry, target);
    extract_archive_payload(ctx, reader, cpio_entry, entry, aes, target);
    LOG_I("archive handler: completed streamed extraction into '" + target.string() + "'");
}

}  // namespace aegis
