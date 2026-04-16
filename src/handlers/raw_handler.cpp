#include "aegis/handlers/raw_handler.h"
#include "aegis/utils.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace aegis {

namespace {

constexpr size_t kBufferSize = 4 * 1024 * 1024; // 4 MiB
constexpr auto kProgressInterval = std::chrono::seconds(5);

int clamp_percent(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

} // namespace

Result<void> write_image_to_device(const std::string& source_path,
                                   const std::string& device_path,
                                   ProgressCallback progress) {
    int src_fd = -1;
    int dst_fd = -1;

    src_fd = ::open(source_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (src_fd < 0) {
        return Result<void>::err("Cannot open source: " + source_path + ": " +
                                 std::string(std::strerror(errno)));
    }

    dst_fd = ::open(device_path.c_str(), O_WRONLY | O_CLOEXEC);
    if (dst_fd < 0) {
        ::close(src_fd);
        return Result<void>::err("Cannot open device: " + device_path + ": " +
                                 std::string(std::strerror(errno)));
    }

    const uint64_t total_size = file_size(source_path);
    uint64_t written = 0;

#ifdef POSIX_FADV_SEQUENTIAL
    (void)::posix_fadvise(src_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    std::vector<uint8_t> buffer(kBufferSize);

    auto last_progress_time = std::chrono::steady_clock::now() - kProgressInterval;
    int last_percent = -1;

    if (progress) {
        progress(0, "Writing image data");
        last_percent = 0;
    }

    while (true) {
        ssize_t rd = ::read(src_fd, buffer.data(), buffer.size());
        if (rd == 0) {
            break;
        }
        if (rd < 0) {
            int saved = errno;
            ::close(src_fd);
            ::close(dst_fd);
            return Result<void>::err("Read error from source: " + source_path + ": " +
                                     std::string(std::strerror(saved)));
        }

        uint8_t* out_ptr = buffer.data();
        ssize_t remaining = rd;

        while (remaining > 0) {
            ssize_t wr = ::write(dst_fd, out_ptr, static_cast<size_t>(remaining));
            if (wr < 0) {
                int saved = errno;
                ::close(src_fd);
                ::close(dst_fd);
                return Result<void>::err("Write error to device: " + device_path + ": " +
                                         std::string(std::strerror(saved)));
            }

            remaining -= wr;
            out_ptr += wr;
            written += static_cast<uint64_t>(wr);
        }

        if (progress && total_size > 0) {
            const int percent =
                clamp_percent(static_cast<int>((written * 100) / total_size));

            const auto now = std::chrono::steady_clock::now();
            const bool time_due = (now - last_progress_time) >= kProgressInterval;
            const bool final_due = (percent == 100);
            const bool meaningful_jump = (percent >= last_percent + 5);

            if (time_due || final_due || meaningful_jump) {
                progress(percent, "Writing image data");
                last_progress_time = now;
                last_percent = percent;
            }
        }
    }

    if (::fsync(dst_fd) != 0) {
        int saved = errno;
        ::close(src_fd);
        ::close(dst_fd);
        return Result<void>::err("fsync failed for device: " + device_path + ": " +
                                 std::string(std::strerror(saved)));
    }

    ::close(src_fd);
    ::close(dst_fd);

    if (progress) {
        progress(100, "Image data written");
    }

    LOG_INFO("Wrote %lu bytes to %s", written, device_path.c_str());
    return Result<void>::ok();
}

} // namespace aegis