#include "aegis/common/io.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <system_error>
#include <vector>

#include <unistd.h>

#include "aegis/core/types.hpp"

namespace aegis {
namespace {

[[noreturn]] void fail_errno(const std::string &message) {
    fail_runtime(message + ": " + std::strerror(errno));
}

void update_checksum(std::uint32_t &checksum, const char *data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        checksum += static_cast<unsigned char>(data[i]);
    }
}

std::size_t checked_size(std::uint64_t value, const char *name) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        fail_runtime(std::string(name) + " is too large for this platform");
    }

    return static_cast<std::size_t>(value);
}

}  // namespace

FileDescriptor::FileDescriptor(int fd) noexcept
    : fd_(fd) {
}

FileDescriptor::~FileDescriptor() {
    reset();
}

FileDescriptor::FileDescriptor(FileDescriptor &&other) noexcept
    : fd_(other.release()) {
}

FileDescriptor &FileDescriptor::operator=(FileDescriptor &&other) noexcept {
    if (this != &other) {
        reset(other.release());
    }

    return *this;
}

int FileDescriptor::get() const noexcept {
    return fd_;
}

bool FileDescriptor::valid() const noexcept {
    return fd_ >= 0;
}

FileDescriptor::operator bool() const noexcept {
    return valid();
}

int FileDescriptor::release() noexcept {
    const int old = fd_;
    fd_ = -1;
    return old;
}

void FileDescriptor::reset(int fd) noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
    }

    fd_ = fd;
}

TempFile::TempFile(std::string_view prefix, std::string_view contents) {
    std::string pattern = "/tmp/";
    pattern.append(prefix);
    pattern.append("XXXXXX");

    std::vector<char> path(pattern.begin(), pattern.end());
    path.push_back('\0');

    const int fd = ::mkstemp(path.data());
    if (fd < 0) {
        fail_errno("mkstemp failed");
    }

    path_ = path.data();
    fd_.reset(fd);

    if (!contents.empty()) {
        write_all_fd(fd_.get(), contents.data(), contents.size());

        if (::lseek(fd_.get(), 0, SEEK_SET) < 0) {
            fail_errno("lseek failed");
        }
    }
}

TempFile::~TempFile() {
    if (!path_.empty()) {
        ::unlink(path_.c_str());
    }
}

TempFile::TempFile(TempFile &&other) noexcept
    : path_(std::move(other.path_)),
      fd_(std::move(other.fd_)) {
    other.path_.clear();
}

TempFile &TempFile::operator=(TempFile &&other) noexcept {
    if (this != &other) {
        if (!path_.empty()) {
            ::unlink(path_.c_str());
        }

        path_ = std::move(other.path_);
        fd_ = std::move(other.fd_);

        other.path_.clear();
    }

    return *this;
}

const std::string &TempFile::path() const noexcept {
    return path_;
}

StreamReader::StreamReader(int input_fd, int tee_fd, ReadProgressCallback progress_callback) noexcept
    : input_fd_(input_fd),
      tee_fd_(tee_fd),
      progress_callback_(progress_callback) {
}

void StreamReader::read_exact(char *dst, std::size_t len) {
    std::size_t done = 0;

    while (done < len) {
        const ssize_t rc = ::read(input_fd_, dst + done, len - done);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }

            fail_errno("read failed");
        }

        if (rc == 0) {
            fail_runtime("unexpected end of stream");
        }

        const auto bytes_read = static_cast<std::size_t>(rc);

        tee(dst + done, bytes_read);

        if (progress_callback_) {
            progress_callback_(bytes_read);
        }
        done += bytes_read;
    }
}

void StreamReader::skip(std::uint64_t len, std::uint32_t *checksum) {
    std::array<char, kIoBufferSize> buffer{};

    while (len > 0) {
        const auto chunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(len, buffer.size())
        );

        read_exact(buffer.data(), chunk);

        if (checksum != nullptr) {
            update_checksum(*checksum, buffer.data(), chunk);
        }

        len -= chunk;
    }
}

std::string StreamReader::read_string(std::uint64_t len, std::uint32_t *checksum) {
    std::string out;
    out.resize(checked_size(len, "stream string"));

    if (!out.empty()) {
        read_exact(out.data(), out.size());

        if (checksum != nullptr) {
            update_checksum(*checksum, out.data(), out.size());
        }
    }

    return out;
}

void StreamReader::tee(const char *data, std::size_t len) {
    if (tee_fd_ >= 0) {
        write_all_fd(tee_fd_, data, len);
    }
}

void write_all_fd(int fd, const char *data, std::size_t len) {
    while (len > 0) {
        const ssize_t rc = ::write(fd, data, len);

        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EPIPE) {
                fail_runtime("write failed: downstream reader closed the stream");
            }

            fail_errno("write failed");
        }

        if (rc == 0) {
            fail_runtime("write failed: write returned 0 bytes");
        }

        const auto written = static_cast<std::size_t>(rc);
        data += written;
        len -= written;
    }
}

void ensure_parent_dir(const fs::path &path) {
    const fs::path parent = path.parent_path();

    if (parent.empty()) {
        return;
    }

    std::error_code ec;
    fs::create_directories(parent, ec);

    if (ec) {
        fail_runtime("cannot create directory '" + parent.string() + "': " + ec.message());
    }
}

}  // namespace aegis