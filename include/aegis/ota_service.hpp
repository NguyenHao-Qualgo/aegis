#pragma once

#include <string>
#include <vector>
#include <memory>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
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
               StateStore stateStore);

    OtaStatus getStatus() const;

    void startInstall(const std::string& bundlePath);
    void markGood();
    void markBad();
    void markActive(const std::string& slot);

    std::string getPrimary() const;
    std::string getBooted() const;

    void resumeAfterBoot();

private:
    OtaContext context_;
};

}  // namespace aegis