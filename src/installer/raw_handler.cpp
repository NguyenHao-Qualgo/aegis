#include "aegis/installer/handlers.hpp"

#include <fcntl.h>
#include <filesystem>
#include <limits.h>
#include <sys/stat.h>
#include <zlib.h>

#include "aegis/common/error.hpp"
#include "aegis/common/logging.hpp"
#include "aegis/core/types.hpp"
#include "aegis/installer/handler_utils.hpp"
#include "aegis/installer/install_context.hpp"
#include "aegis/io/io.hpp"

namespace aegis {

namespace {

int blkprotect(const fs::path& device, bool on) {
    char abs_path[PATH_MAX] = {};
    constexpr char unprot_char = '0';
    constexpr char prot_char = '1';

    if (device.string().rfind("/dev/", 0) != 0) {
        return 0;
    }

    struct stat sb {};
    if (::stat(device.c_str(), &sb) != 0 || !S_ISBLK(sb.st_mode)) {
        return 0;
    }

    if (::realpath(device.c_str(), abs_path) == nullptr) {
        return -errno;
    }

    std::string sysfs_path = "/sys/class/block/";
    sysfs_path += abs_path + 5;
    sysfs_path += "/force_ro";

    if (::access(sysfs_path.c_str(), W_OK) != 0) {
        return 0;
    }

    FileDescriptor fd(::open(sysfs_path.c_str(), O_RDWR));
    if (!fd) {
        return -EBADF;
    }

    char current = 0;
    if (::read(fd.get(), &current, 1) != 1) {
        return -EIO;
    }

    const char requested = on ? prot_char : unprot_char;
    if (requested == current) {
        return 0;
    }
    if (::lseek(fd.get(), 0, SEEK_SET) < 0) {
        return -EIO;
    }
    if (::write(fd.get(), &requested, 1) != 1) {
        return -EIO;
    }
    return 1;
}

class BlockProtectionGuard {
public:
    explicit BlockProtectionGuard(fs::path device) : device_(std::move(device)) {
        status_ = blkprotect(device_, false);
        if (status_ < 0) {
            fail_runtime("failed to disable block protection for " + device_.string());
        }
    }

    ~BlockProtectionGuard() {
        if (status_ == 1) {
            (void)blkprotect(device_, true);
        }
    }

private:
    fs::path device_;
    int status_ = 0;
};

class GzipInflateSink {
public:
    GzipInflateSink(const InstallContext& ctx, int out_fd)
        : ctx_(ctx), out_fd_(out_fd) {
        zstream_.zalloc = Z_NULL;
        zstream_.zfree = Z_NULL;
        zstream_.opaque = Z_NULL;

        const int rc = ::inflateInit2(&zstream_, 16 + MAX_WBITS);
        if (rc != Z_OK) {
            fail_runtime("failed to initialize zlib inflater");
        }
        initialized_ = true;
    }

    ~GzipInflateSink() {
        if (initialized_) {
            ::inflateEnd(&zstream_);
        }
    }

    void write(const char* data, std::size_t len) {
        ctx_.check_cancel();

        zstream_.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data));
        zstream_.avail_in = static_cast<uInt>(len);

        while (zstream_.avail_in > 0) {
            inflate_chunk(Z_NO_FLUSH);
        }
    }

    void finish() {
        ctx_.check_cancel();

        int rc = Z_OK;
        while (rc != Z_STREAM_END) {
            rc = inflate_chunk(Z_FINISH);
        }
    }

private:
    int inflate_chunk(int flush_mode) {
        std::array<unsigned char, kIoBufferSize> outbuf{};
        zstream_.next_out = outbuf.data();
        zstream_.avail_out = static_cast<uInt>(outbuf.size());

        const int rc = ::inflate(&zstream_, flush_mode);
        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
            fail_runtime("zlib inflate failed with code " + std::to_string(rc));
        }

        const std::size_t produced = outbuf.size() - zstream_.avail_out;
        if (produced > 0) {
            write_all_checked(out_fd_, reinterpret_cast<const char*>(outbuf.data()), produced, ctx_);
        }

        if (flush_mode == Z_FINISH && rc == Z_BUF_ERROR && produced == 0) {
            fail_runtime("truncated gzip payload");
        }

        return rc;
    }

    const InstallContext& ctx_;
    int out_fd_;
    z_stream zstream_{};
    bool initialized_ = false;
};

}  // namespace

void RawHandler::install(const InstallContext& ctx,
                         StreamReader &reader,
                         const CpioEntry &cpio_entry,
                         const ManifestEntry &entry,
                         const AesMaterial *aes) {
    ctx.check_cancel();

    if (entry.device.empty()) {
        fail_runtime("raw image missing device for " + entry.filename);
    }

    const fs::path target = entry.device;
    LOG_I("raw handler: target device='" + target.string() + "', mode=direct stream write");

    BlockProtectionGuard protection(target);
    FileDescriptor out(::open(target.c_str(), O_RDWR));
    if (!out) {
        fail_runtime("cannot open device " + target.string());
    }

    PayloadStreamer streamer(ctx);
    const bool compressed = entry.compress == "zlib";
    if (!entry.compress.empty() && !compressed) {
        fail_runtime("unsupported raw compression '" + entry.compress + "' for " + entry.filename);
    }

    if (compressed) {
        GzipInflateSink inflate_sink(ctx, out.get());
        auto sink = [&](const char* data, std::size_t len) {
            inflate_sink.write(data, len);
        };
        stream_payload(streamer, reader, cpio_entry, entry, aes, sink);

        inflate_sink.finish();
    } else {
        auto sink = [&](const char* data, std::size_t len) {
            write_all_checked(out.get(), data, len, ctx);
        };
        stream_payload(streamer, reader, cpio_entry, entry, aes, sink);
    }

    ctx.check_cancel();
    if (::fsync(out.get()) != 0) {
        fail_runtime("failed to fsync raw device " + target.string());
    }

    LOG_I("raw handler: completed direct stream write to '" + target.string() + "'");
}

}  // namespace aegis
