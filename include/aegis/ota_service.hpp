#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "aegis/bootloader/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/gcs_client.hpp"
#include "aegis/ota_context.hpp"
#include "aegis/ota_state_machine.hpp"
#include "aegis/state_store.hpp"
#include "aegis/types.hpp"
#include "aegis/update_handler.hpp"

namespace aegis {

class OtaService {
public:
    OtaService(OtaConfig config,
               std::unique_ptr<IBootControl> bootControl,
               std::unique_ptr<IBundleVerifier> verifier,
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
    OtaStateMachine machine_;
};

}  // namespace aegis
