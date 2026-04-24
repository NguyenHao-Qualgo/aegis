#include "aegis/core/ota_context.hpp"

#include <stdexcept>

namespace aegis {

OtaContext::OtaContext(OtaConfig config,
                       std::unique_ptr<IBootControl> bootControl,
                       std::shared_ptr<IGcsClient> gcsClient)
    : config_(std::move(config)),
      bootControl_(std::move(bootControl)),
      gcsClient_(std::move(gcsClient)) {
    if (!bootControl_) {
        throw std::invalid_argument("OtaContext requires a boot control implementation");
    }
}

const OtaConfig& OtaContext::config() const {
    return config_;
}

const IBootControl& OtaContext::bootControl() const {
    return *bootControl_;
}

IGcsClient* OtaContext::gcsClient() const {
    return gcsClient_.get();
}

}  // namespace aegis
