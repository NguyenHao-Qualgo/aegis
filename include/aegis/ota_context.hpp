#pragma once

#include <memory>
#include <string>
#include <vector>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/config.hpp"
#include "aegis/gcs_client.hpp"
#include "aegis/types.hpp"
#include "aegis/update_handler.hpp"

namespace aegis {

class OtaContext {
public:
    OtaContext(OtaConfig config,
               std::unique_ptr<IBootControl> bootControl,
               std::unique_ptr<IBundleVerifier> verifier,
               std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
               std::shared_ptr<IGcsClient> gcsClient = nullptr);

    const OtaConfig& config() const;
    const IBootControl& bootControl() const;
    const IBundleVerifier& verifier() const;
    IGcsClient* gcsClient() const;
    const IUpdateHandler& updateHandlerFor(const std::string& imageType) const;

private:
    OtaConfig config_;
    std::unique_ptr<IBootControl> bootControl_;
    std::unique_ptr<IBundleVerifier> verifier_;
    std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers_;
    std::shared_ptr<IGcsClient> gcsClient_;
};

}  // namespace aegis
