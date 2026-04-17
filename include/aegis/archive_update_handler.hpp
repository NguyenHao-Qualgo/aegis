#pragma once

#include "aegis/update_handler.hpp"

namespace aegis {

class ArchiveUpdateHandler : public IUpdateHandler {
public:
    bool supportsImageType(const std::string& imageType) const override;
    void install(const std::string& payloadPath, const SlotConfig& slot, const std::string& workDir) const override;
};

}  // namespace aegis
