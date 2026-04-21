#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <cerrno>
#include <sys/types.h>
#include <unistd.h>

#include "aegis/types.hpp"

namespace aegis {

namespace fs = std::filesystem;

// Declarations for functions defined in io.cpp
void write_all_fd(int fd, const char *data, std::size_t len);
void ensure_parent_dir(const fs::path &path);
int  wait_child(pid_t pid, const std::string &name);
void reap_child(pid_t pid);

class FileDescriptor {
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd) : fd_(fd) {}
    ~FileDescriptor() { reset(); }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    FileDescriptor(FileDescriptor &&other) noexcept : fd_(other.release()) {}
    FileDescriptor &operator=(FileDescriptor &&other) noexcept {
        if (this != &other) { reset(other.release()); }
        return *this;
    }

    int get() const { return fd_; }
    explicit operator bool() const { return fd_ >= 0; }
    int release() { const int old = fd_; fd_ = -1; return old; }
    void reset(int fd = -1) {
        if (fd_ >= 0) { ::close(fd_); }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

class TempFile {
public:
    TempFile(const std::string &prefix, const std::string &contents = {}) {
        std::string pattern = "/tmp/" + prefix + "XXXXXX";
        std::vector<char> path(pattern.begin(), pattern.end());
        path.push_back('\0');
        int fd = ::mkstemp(path.data());
        if (fd < 0) { fail_runtime("mkstemp failed"); }
        path_ = path.data();
        fd_.reset(fd);
        if (!contents.empty()) {
            write_raw(fd_.get(), contents.data(), contents.size());
            ::lseek(fd_.get(), 0, SEEK_SET);
        }
    }
    ~TempFile() { if (!path_.empty()) { ::unlink(path_.c_str()); } }
    const std::string &path() const { return path_; }

private:
    static void write_raw(int fd, const char *data, std::size_t len) {
        while (len > 0) {
            const ssize_t rc = ::write(fd, data, len);
            if (rc < 0) { fail_runtime("write failed"); }
            data += rc;
            len -= static_cast<std::size_t>(rc);
        }
    }
    std::string path_;
    FileDescriptor fd_;
};

class StreamReader {
public:
    StreamReader(int input_fd, int tee_fd = -1) : input_fd_(input_fd), tee_fd_(tee_fd) {}

    void read_exact(char *dst, std::size_t len) {
        std::size_t done = 0;
        while (done < len) {
            const ssize_t rc = ::read(input_fd_, dst + done, len - done);
            if (rc < 0) { fail_runtime(std::string("read failed: ") + std::strerror(errno)); }
            if (rc == 0) { fail_runtime("unexpected end of stream"); }
            tee(dst + done, static_cast<std::size_t>(rc));
            done += static_cast<std::size_t>(rc);
        }
    }

    void skip(std::uint64_t len, std::uint32_t *checksum = nullptr) {
        std::array<char, kIoBufferSize> buffer{};
        while (len > 0) {
            const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(len, buffer.size()));
            read_exact(buffer.data(), chunk);
            if (checksum) {
                for (std::size_t i = 0; i < chunk; ++i) {
                    *checksum += static_cast<unsigned char>(buffer[i]);
                }
            }
            len -= chunk;
        }
    }

    std::string read_string(std::uint64_t len, std::uint32_t *checksum = nullptr) {
        std::string out;
        out.resize(static_cast<std::size_t>(len));
        if (len > 0) {
            read_exact(out.data(), static_cast<std::size_t>(len));
            if (checksum) {
                for (unsigned char c : out) { *checksum += c; }
            }
        }
        return out;
    }

private:
    void tee(const char *data, std::size_t len) {
        if (tee_fd_ >= 0) { write_all_fd(tee_fd_, data, len); }
    }
    int input_fd_;
    int tee_fd_;
};

struct ChildProcess {
    pid_t pid = -1;
    FileDescriptor stdin_fd;
    FileDescriptor stdout_fd;
};

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

ChildProcess spawn_process(const std::vector<std::string> &args,
                           bool connect_stdin,
                           bool connect_stdout,
                           const std::string &chdir_to = {});

}  // namespace aegis
