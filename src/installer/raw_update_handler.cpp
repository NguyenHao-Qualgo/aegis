#include "aegis/installer/raw_update_handler.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <vector>

#include "aegis/bundle/bundle_extractor.hpp"
#include "aegis/util.hpp"

namespace aegis {

namespace {

class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) : fd_(fd) {}
    ~FileDescriptor() { if (fd_ >= 0) ::close(fd_); }
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

void RawUpdateHandler::installFromBundle(const std::string& bundlePath,
                                          std::uint64_t bundlePayloadSize,
                                          const std::string& entryName,
                                          const std::string& expectedSha256,
                                          const SlotConfig& slot,
                                          const std::string& /*workDir*/) const {
    if (slot.type != SlotType::Ext4) {
        throw std::runtime_error("Raw handler only supports ext4 slots");
    }

    logInfo("Streaming '" + entryName + "' -> " + slot.device);

    BundleExtractor extractor;
    extractor.streamEntry(bundlePath, bundlePayloadSize, entryName, slot.device, expectedSha256);
}

}  // namespace aegis
