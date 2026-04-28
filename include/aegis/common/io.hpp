#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <functional>

namespace aegis {

namespace fs = std::filesystem;

void write_all_fd(int fd, const char *data, std::size_t len);
void ensure_parent_dir(const fs::path &path);

class FileDescriptor {
public:
    FileDescriptor() noexcept = default;
    explicit FileDescriptor(int fd) noexcept;

    ~FileDescriptor();

    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;

    FileDescriptor(FileDescriptor &&other) noexcept;
    FileDescriptor &operator=(FileDescriptor &&other) noexcept;

    [[nodiscard]] int get() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
    explicit operator bool() const noexcept;

    [[nodiscard]] int release() noexcept;
    void reset(int fd = -1) noexcept;

private:
    int fd_ = -1;
};

class TempFile {
public:
    explicit TempFile(std::string_view prefix, std::string_view contents = {});
    ~TempFile();

    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;

    TempFile(TempFile &&other) noexcept;
    TempFile &operator=(TempFile &&other) noexcept;

    [[nodiscard]] const std::string &path() const noexcept;

private:
    std::string path_;
    FileDescriptor fd_;
};

class StreamReader {
public:
    using ReadProgressCallback = std::function<void(std::size_t)>;
    StreamReader(int input_fd, int tee_fd = -1, ReadProgressCallback progress_callback = {}) noexcept;

    void read_exact(char *dst, std::size_t len);
    void skip(std::uint64_t len, std::uint32_t *checksum = nullptr);
    std::string read_string(std::uint64_t len, std::uint32_t *checksum = nullptr);

private:
    void tee(const char *data, std::size_t len);

    int input_fd_;
    int tee_fd_;
    ReadProgressCallback progress_callback_;
};

}  // namespace aegis