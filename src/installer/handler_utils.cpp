#include "aegis/installer/handler_utils.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <string>
#include <zlib.h>

namespace aegis {

namespace {

class GzipInflateToFdSink {
public:
    GzipInflateToFdSink(const InstallContext& ctx,
                       int out_fd,
                       std::string broken_pipe_message)
        : ctx_(ctx),
          out_fd_(out_fd),
          broken_pipe_message_(std::move(broken_pipe_message)) {
        zstream_.zalloc = Z_NULL;
        zstream_.zfree = Z_NULL;
        zstream_.opaque = Z_NULL;

        const int rc = ::inflateInit2(&zstream_, 16 + MAX_WBITS);
        if (rc != Z_OK) {
            fail_runtime("failed to initialize zlib inflater");
        }
        initialized_ = true;
    }

    ~GzipInflateToFdSink() {
        if (initialized_) {
            ::inflateEnd(&zstream_);
        }
    }

    void write(const char* data, const std::size_t len) {
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
    int inflate_chunk(const int flush_mode) {
        std::array<unsigned char, kIoBufferSize> outbuf{};
        zstream_.next_out = outbuf.data();
        zstream_.avail_out = static_cast<uInt>(outbuf.size());

        const int rc = ::inflate(&zstream_, flush_mode);
        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
            fail_runtime("zlib inflate failed with code " + std::to_string(rc));
        }

        const std::size_t produced = outbuf.size() - zstream_.avail_out;
        if (produced > 0) {
            write_all_checked(out_fd_,
                              reinterpret_cast<const char*>(outbuf.data()),
                              produced,
                              ctx_,
                              broken_pipe_message_);
        }

        if (flush_mode == Z_FINISH && rc == Z_BUF_ERROR && produced == 0) {
            fail_runtime("truncated gzip payload");
        }

        return rc;
    }

    const InstallContext& ctx_;
    int out_fd_;
    std::string broken_pipe_message_;
    z_stream zstream_{};
    bool initialized_ = false;
};

}  // namespace

void write_all_checked(int fd,
                       const char* data,
                       std::size_t len,
                       const InstallContext& ctx,
                       const std::string& broken_pipe_message) {
    while (len > 0) {
        ctx.check_cancel();

        const ssize_t rc = ::write(fd, data, len);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EPIPE && !broken_pipe_message.empty()) {
                fail_runtime(broken_pipe_message);
            }
            fail_runtime(std::string("write failed: ") + std::strerror(errno));
        }

        data += rc;
        len -= static_cast<std::size_t>(rc);
    }
}

void stream_payload_to_fd(PayloadStreamer& streamer,
                          StreamReader& reader,
                          const CpioEntry& cpio_entry,
                          const ManifestEntry& entry,
                          const AesMaterial* aes,
                          const InstallContext& ctx,
                          const int out_fd,
                          const std::string& broken_pipe_message) {
    const bool compressed = entry.compress == "zlib";
    if (!entry.compress.empty() && !compressed) {
        fail_runtime("unsupported compression '" + entry.compress + "' for " + entry.filename);
    }

    if (compressed) {
        GzipInflateToFdSink inflate_sink(ctx, out_fd, broken_pipe_message);
        auto sink = [&](const char* data, const std::size_t len) {
            inflate_sink.write(data, len);
        };
        stream_payload(streamer, reader, cpio_entry, entry, aes, sink);
        inflate_sink.finish();
        return;
    }

    auto sink = [&](const char* data, const std::size_t len) {
        write_all_checked(out_fd, data, len, ctx, broken_pipe_message);
    };
    stream_payload(streamer, reader, cpio_entry, entry, aes, sink);
}

}  // namespace aegis
