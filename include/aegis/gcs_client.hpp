#pragma once

#include <optional>
#include <string>

#include "aegis/types.hpp"

namespace aegis {

struct GcsUpdateInfo {
    std::string bundleUrl;
    std::string version;
};

class IGcsClient {
public:
    virtual ~IGcsClient() = default;

    // Check for a pending OTA update. Returns nullopt when none is available.
    virtual std::optional<GcsUpdateInfo> checkForUpdate() = 0;

    // Report current OTA status back to the backend.
    virtual void reportStatus(const OtaStatus& status) = 0;
};

}  // namespace aegis
