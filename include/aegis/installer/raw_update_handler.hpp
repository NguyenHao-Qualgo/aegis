#pragma once

#include "aegis/installer/update_handler.hpp"

namespace aegis {

class RawUpdateHandler : public IUpdateHandler {
public:
    bool supportsImageType(const std::string& imageType) const override;
    void installFromBundle(const std::string& bundlePath, std::uint64_t bundlePayloadSize,
                           const std::string& entryName, const std::string& expectedSha256,
                           const SlotConfig& slot, const std::string& workDir) const override;
};

}  // namespace aegis
