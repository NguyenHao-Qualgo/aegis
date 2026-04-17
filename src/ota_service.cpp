#include "aegis/ota_service.hpp"

#include <thread>
#include <utility>

#include "aegis/ota_event.hpp"
#include "aegis/util.hpp"

namespace aegis {

OtaService::OtaService(OtaConfig config,
                       BootControl bootControl,
                       BundleVerifier verifier,
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
        logWarn("Discarding pending reboot state and starting a new install");
        context_.discardPendingRebootState();
    }

    std::thread([this, bundlePath]() {
        context_.dispatch(OtaEvent{
            OtaEvent::Type::StartInstall,
            bundlePath,
            ""
        });
    }).detach();
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