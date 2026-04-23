#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "aegis/bootloader/boot_control.hpp"
#include "aegis/core/gcs_client.hpp"
#include "aegis/core/ota_context.hpp"
#include "aegis/core/ota_state_machine.hpp"
#include "aegis/common/state_store.hpp"
#include "aegis/core/types.hpp"

namespace aegis {

class OtaService {
public:
    OtaService(OtaConfig config,
               std::unique_ptr<IBootControl> bootControl,
               StateStore stateStore,
               std::shared_ptr<IGcsClient> gcsClient = nullptr);

    OtaStatus getStatus() const;

    void startInstall(const std::string& bundlePath);
    void cancelInstall();
    void markGood();
    void markBad();
    void markActive(const std::string& slot);

    std::string getPrimary() const;
    std::string getBooted() const;

    void resumeAfterBoot();
    void setStatusChangedCallback(std::function<void(const OtaStatus&)> cb);

private:
    bool isInstallActive(const OtaStatus& status) const;
    void reapFinishedInstallLocked(const OtaStatus& status);

    void runInstall(std::stop_token stop, std::string bundlePath);

    OtaStateMachine machine_;
    mutable std::mutex installMutex_;
    std::jthread installThread_;
};

}  // namespace aegis
