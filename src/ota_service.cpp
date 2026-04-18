#include "aegis/ota_service.hpp"

#include <thread>
#include <utility>
#include <memory>

#include "aegis/ota_event.hpp"
#include "aegis/util.hpp"
#include "aegis/states/idle_state.hpp"

namespace aegis {

OtaService::OtaService(OtaConfig config,
                       std::unique_ptr<IBootControl> bootControl,
                       std::unique_ptr<IBundleVerifier> verifier,
                       std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
                       StateStore stateStore,
                       std::shared_ptr<IGcsClient> gcsClient)
    : context_(std::move(config),
               std::move(bootControl),
               std::move(verifier),
               std::move(updateHandlers),
               std::move(stateStore),
               std::move(gcsClient)) {
}

OtaStatus OtaService::getStatus() const {
    return context_.getStatus();
}

void OtaService::startInstall(const std::string& bundlePath) {
    const auto status = context_.getStatus();

    if (status.state == OtaState::Reboot) {
        logWarn("Cancelling pending reboot to start a new install");
        context_.setState(std::make_unique<IdleState>());
    }

    context_.dispatch(OtaEvent{
        OtaEvent::Type::StartInstall,
        bundlePath,
        ""
    });
}

void OtaService::markGood() {
    context_.dispatch(OtaEvent{
        OtaEvent::Type::MarkGood,
        "",
        ""
    });
}

void OtaService::markBad() {
    context_.dispatch(OtaEvent{
        OtaEvent::Type::MarkBad,
        "",
        ""
    });
}

void OtaService::resumeAfterBoot() {
    context_.dispatch(OtaEvent{
        OtaEvent::Type::ResumeAfterBoot,
        "",
        ""
    });
}

void OtaService::markActive(const std::string& slot) {
    context_.markActive(slot);
}

std::string OtaService::getPrimary() const {
    return context_.getPrimary();
}

std::string OtaService::getBooted() const {
    return context_.getBooted();
}

void OtaService::setStatusChangedCallback(std::function<void(const OtaStatus&)> cb) {
    context_.setStatusChangedCallback(std::move(cb));
}

}  // namespace aegis