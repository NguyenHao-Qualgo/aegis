#include "aegis/installer/file_handler.hpp"

#include <fcntl.h>
#include <filesystem>

#include "aegis/common/error.hpp"
#include "aegis/common/io.hpp"
#include "aegis/common/logging.hpp"
#include "aegis/installer/handler_utils.hpp"
#include "aegis/installer/install_context.hpp"

namespace aegis {

void FileHandler::install(const InstallContext& ctx,
                          StreamReader& reader,
                          const CpioEntry& cpio_entry,
                          const ManifestEntry& entry,
                          const AesMaterial* aes) {
    ctx.check_cancel();

    if (entry.path.empty()) {
        fail_runtime("file entry missing path for " + entry.filename);
    }

    const fs::path target = entry.path;
    ensure_parent_dir(target);

    LOG_I("file target: " + target.string());

    FileDescriptor out(::open(target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644));
    if (!out) {
        fail_runtime("cannot open file " + target.string());
    }

    PayloadStreamer streamer(ctx);
    stream_payload_to_fd(streamer, reader, cpio_entry, entry, aes, ctx, out.get());

    ctx.check_cancel();
    if (::fsync(out.get()) != 0) {
        fail_runtime("failed to fsync file " + target.string());
    }

    LOG_I("file done: " + target.string());
}

}  // namespace aegis
