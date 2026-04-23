#pragma once

#include <memory>
#include <string>
#include <vector>

#include "aegis/bootloader/boot_control.hpp"
#include "aegis/common/config.hpp"
#include "aegis/core/gcs_client.hpp"
#include "aegis/core/types.hpp"

namespace aegis {

class OtaContext {
public:
    OtaContext(OtaConfig config,
               std::unique_ptr<IBootControl> bootControl,
               std::shared_ptr<IGcsClient> gcsClient = nullptr);

    const OtaConfig& config() const;
    const IBootControl& bootControl() const;
    IGcsClient* gcsClient() const;

private:
    OtaConfig config_;
    std::unique_ptr<IBootControl> bootControl_;
    std::shared_ptr<IGcsClient> gcsClient_;
};

}  // namespace aegis
