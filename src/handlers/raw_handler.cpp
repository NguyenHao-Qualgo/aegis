#include "aegis/handlers/raw_handler.h"
#include "aegis/utils.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fs.h>
#endif

namespace aegis {

namespace {

class FileDescriptor {
  public:
    explicit FileDescriptor(int fd = -1) : fd_(fd) {}
    ~FileDescriptor() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    [[nodiscard]] int get() const {
        return fd_;
    }

    [[nodiscard]] bool valid() const {
        return fd_ >= 0;
    }

  private:
    int fd_;
};

constexpr size_t kBufferSize = 4 * 1024 * 1024; // 4 MiB
constexpr auto kProgressInterval = std::chrono::seconds(5);

uint64_t bytes_to_mib(uint64_t bytes) {
    return bytes / (1024 * 1024);
}

int clamp_percent(int value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

Result<uint64_t> get_fd_size_bytes(int fd) {
    struct stat st {};
    if (fstat(fd, &st) != 0) {
        return Result<uint64_t>::err("fstat failed: " + std::string(std::strerror(errno)));
    }

    if (S_ISREG(st.st_mode)) {
        return Result<uint64_t>::ok(static_cast<uint64_t>(st.st_size));
    }

#if defined(__linux__)
    if (S_ISBLK(st.st_mode)) {
        uint64_t size = 0;
        if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
            return Result<uint64_t>::ok(size);
        }
        return Result<uint64_t>::err("BLKGETSIZE64 failed: " +
                                     std::string(std::strerror(errno)));
    }
#endif

    return Result<uint64_t>::err("Unsupported file type for size query");
}

} // namespace

Result<void> write_image_to_device(const std::string& source_path,
                                   const std::string& device_path,
                                   const std::string& label,
                                   ProgressCallback progress) {
    FileDescriptor src_fd(::open(source_path.c_str(), O_RDONLY | O_CLOEXEC));
    if (!src_fd.valid()) {
        return Result<void>::err("Cannot open source: " + source_path + ": " +
                                 std::string(std::strerror(errno)));
    }

    FileDescriptor dst_fd(::open(device_path.c_str(), O_WRONLY | O_CLOEXEC));
    if (!dst_fd.valid()) {
        return Result<void>::err("Cannot open device: " + device_path + ": " +
                                 std::string(std::strerror(errno)));
    }

    auto source_size_res = get_fd_size_bytes(src_fd.get());
    if (!source_size_res) {
        return Result<void>::err("Failed to determine source size: " + source_size_res.error());
    }

    auto device_size_res = get_fd_size_bytes(dst_fd.get());
    if (!device_size_res) {
        return Result<void>::err("Failed to determine target device size: " +
                                 device_size_res.error());
    }

    const uint64_t total_size = source_size_res.value();
    const uint64_t device_size = device_size_res.value();

    if (device_size > 0 && total_size > device_size) {
        return Result<void>::err("Target device '" + device_path + "' is too small: image=" +
                                 std::to_string(total_size) + " bytes, device=" +
                                 std::to_string(device_size) + " bytes");
    }

#ifdef POSIX_FADV_SEQUENTIAL
    (void)::posix_fadvise(src_fd.get(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    std::vector<uint8_t> buffer(kBufferSize);
    uint64_t written = 0;
    int last_percent = -1;
    auto last_progress_time = std::chrono::steady_clock::now() - kProgressInterval;

    if (progress) {
        progress(0, "Writing image " + label);
    }

    while (true) {
        ssize_t rd = ::read(src_fd.get(), buffer.data(), buffer.size());
        if (rd == 0) {
            break;
        }
        if (rd < 0) {
            int saved = errno;
            return Result<void>::err("Read error from source: " + source_path + ": " +
                                     std::string(std::strerror(saved)));
        }

        uint8_t* out_ptr = buffer.data();
        ssize_t remaining = rd;

        while (remaining > 0) {
            ssize_t wr = ::write(dst_fd.get(), out_ptr, static_cast<size_t>(remaining));
            if (wr < 0) {
                int saved = errno;
                return Result<void>::err("Write error to device '" + device_path +
                                         "' at offset " + std::to_string(written) + ": " +
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
                progress(
                    percent,
                    "Writing image " + label + " (" +
                        std::to_string(bytes_to_mib(written)) + " / " +
                        std::to_string(bytes_to_mib(total_size)) + " MiB)");
                last_progress_time = now;
                last_percent = percent;
            }
        }
    }

    if (::fsync(dst_fd.get()) != 0) {
        int saved = errno;
        return Result<void>::err("fsync failed for device: " + device_path + ": " +
                                 std::string(std::strerror(saved)));
    }

    if (progress) {
        progress(100, "Image " + label + " written");
    }

    LOG_INFO("Wrote %lu bytes to %s", total_size, device_path.c_str());
    return Result<void>::ok();
}

} // namespace aegis