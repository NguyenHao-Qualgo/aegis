#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/gcs_client.hpp"
#include "aegis/ota_context.hpp"
#include "aegis/state_store.hpp"
#include "aegis/types.hpp"
#include "aegis/update_handler.hpp"

namespace aegis {

class OtaService {
public:
    OtaService(OtaConfig config,
               BootControl bootControl,
               BundleVerifier verifier,
               std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
               StateStore stateStore,
               std::shared_ptr<IGcsClient> gcsClient = nullptr);

    OtaStatus getStatus() const;

    void startInstall(const std::string& bundlePath);
    void markGood();
    void markBad();
    void markActive(const std::string& slot);

    std::string getPrimary() const;
    std::string getBooted() const;

    void resumeAfterBoot();
    void setStatusChangedCallback(std::function<void(const OtaStatus&)> cb);
private:
    OtaContext context_;
};

}  // namespace aegis