#include "aegis/installer/raw_update_handler.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

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

    int get() const { return fd_; }

private:
    int fd_;
};

}  // namespace

bool RawUpdateHandler::supportsImageType(const std::string& imageType) const {
    return imageType == "raw";
}

void RawUpdateHandler::install(const std::string& payloadPath, const SlotConfig& slot,
                               const std::string& /*workDir*/) const {
    if (slot.type != SlotType::Ext4) {
        throw std::runtime_error("Raw handler only supports ext4 slots");
    }

    FileDescriptor input(::open(payloadPath.c_str(), O_RDONLY));
    if (input.get() < 0) {
        throw std::runtime_error("Cannot open raw payload: " + payloadPath);
    }

    FileDescriptor output(::open(slot.device.c_str(), O_WRONLY));
    if (output.get() < 0) {
        throw std::runtime_error("Cannot open target device: " + slot.device);
    }

    std::vector<char> buffer(1024 * 1024);
    while (true) {
        const auto bytesRead = ::read(input.get(), buffer.data(), buffer.size());
        if (bytesRead < 0) {
            throw std::runtime_error("Failed while reading raw payload");
        }
        if (bytesRead == 0) {
            break;
        }

        ssize_t written = 0;
        while (written < bytesRead) {
            const auto bytesWritten = ::write(output.get(), buffer.data() + written, bytesRead - written);
            if (bytesWritten < 0) {
                throw std::runtime_error("Failed while writing target device");
            }
            written += bytesWritten;
        }
    }

    if (::fsync(output.get()) != 0) {
        throw std::runtime_error("Failed to flush target device");
    }
    ::sync();
}

}  // namespace aegis
