#pragma once

#include <cstdint>
#include <string>

#include "aegis/types.hpp"

namespace aegis {

class IUpdateHandler {
public:
    virtual ~IUpdateHandler() = default;

    virtual bool supportsImageType(const std::string& imageType) const = 0;

    // Install by streaming a named entry directly from the bundle tar.gz.
    // Avoids writing an intermediate payload file when the partition holding
    // workDir is too small (e.g. raw ext4 images on a small data partition).
    virtual void installFromBundle(const std::string& bundlePath,
                                   std::uint64_t bundlePayloadSize,
                                   const std::string& entryName,
                                   const std::string& expectedSha256,
                                   const SlotConfig& slot,
                                   const std::string& workDir) const = 0;
};

}  // namespace aegis
