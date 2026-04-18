#include "aegis/ota_context.hpp"

#include <stdexcept>

namespace aegis {

OtaContext::OtaContext(OtaConfig config,
                       std::unique_ptr<IBootControl> bootControl,
                       std::unique_ptr<IBundleVerifier> verifier,
                       std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
                       std::shared_ptr<IGcsClient> gcsClient)
    : config_(std::move(config)),
      bootControl_(std::move(bootControl)),
      verifier_(std::move(verifier)),
      updateHandlers_(std::move(updateHandlers)),
      gcsClient_(std::move(gcsClient)) {}

const OtaConfig& OtaContext::config() const {
    return config_;
}

const IBootControl& OtaContext::bootControl() const {
    return *bootControl_;
}

const IBundleVerifier& OtaContext::verifier() const {
    return *verifier_;
}

IGcsClient* OtaContext::gcsClient() const {
    return gcsClient_.get();
}

const IUpdateHandler& OtaContext::updateHandlerFor(const std::string& imageType) const {
    for (const auto& handler : updateHandlers_) {
        if (handler->supportsImageType(imageType)) {
            return *handler;
        }
    }
    throw std::runtime_error("Unsupported payload type: " + imageType);
}

}  // namespace aegis
