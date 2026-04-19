#pragma once

#include <string>

#include "aegis/types.hpp"

namespace aegis {

class IUpdateHandler {
public:
    virtual ~IUpdateHandler() = default;

    virtual bool supportsImageType(const std::string& imageType) const = 0;
    virtual void install(const std::string& payloadPath, const SlotConfig& slot, const std::string& workDir) const = 0;
};

}  // namespace aegis
