#include "aegis/ota_service.hpp"

#include <thread>
#include <utility>
#include <memory>

#include "aegis/ota_event.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

OtaService::OtaService(OtaConfig config,
                       std::unique_ptr<IBootControl> bootControl,
                       StateStore stateStore,
                       std::shared_ptr<IGcsClient> gcsClient)
    : machine_(OtaContext(std::move(config),
                          std::move(bootControl),
                          std::move(gcsClient)),
               std::move(stateStore)) {
}

OtaStatus OtaService::getStatus() const {
    return machine_.getStatus();
}

void OtaService::startInstall(const std::string& bundlePath) {
    const auto status = machine_.getStatus();

    if (status.state == OtaState::Reboot) {
        logWarn("Cancelling pending reboot to start a new install");
        machine_.transitionTo(std::make_unique<IdleState>());
    }

    machine_.dispatch(OtaEvent{
        OtaEvent::Type::StartInstall,
        bundlePath,
        ""
    });
}

void OtaService::resumeAfterBoot() {
    machine_.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});
}

void OtaService::markActive(const std::string& slot) {
    machine_.markActive(slot);
}

std::string OtaService::getPrimary() const {
    return machine_.bootControl().getPrimarySlot();
}

std::string OtaService::getBooted() const {
    return machine_.bootControl().getBootedSlot();
}

void OtaService::setStatusChangedCallback(std::function<void(const OtaStatus&)> cb) {
    machine_.setStatusChangedCallback(std::move(cb));
}

}  // namespace aegis
