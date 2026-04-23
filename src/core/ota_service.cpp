#include "aegis/core/ota_service.hpp"

#include <thread>
#include <utility>
#include <memory>

#include "aegis/core/ota_event.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/common/util.hpp"

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

bool OtaService::isInstallActive(const OtaStatus& status) const {
    return status.state == OtaState::Download || status.state == OtaState::Install;
}

void OtaService::dispatchEvent(const OtaEvent& event) {
    machine_.dispatch(event);
}

void OtaService::reapFinishedInstallLocked(const OtaStatus& status) {
    if (installThread_.joinable() && !isInstallActive(status)) {
        installThread_.join();
    }
}

void OtaService::startInstall(const std::string& bundlePath) {
    std::scoped_lock lock(installMutex_);
    const auto status = machine_.getStatus();

    if (status.state == OtaState::Reboot) {
        LOG_W("Cancelling pending reboot to start a new install");
        machine_.transitionToIdle();
    }

    reapFinishedInstallLocked(status);

    if (installThread_.joinable()) {
        throw std::runtime_error("Install already in progress");
    }

    installThread_ = std::jthread([this, bundlePath](std::stop_token stop) {
        try {
            LOG_I("Install thread started");
            runInstall(stop, bundlePath);
            LOG_I("Install finished");
        } catch (const std::exception& e) {
            LOG_E(std::string("Install failed: ") + e.what());
            machine_.clearInstallStopToken();
        }
    });
}

void OtaService::runInstall(std::stop_token stop, std::string bundlePath) {
    machine_.setInstallStopToken(stop);
    try {
        dispatchEvent(OtaEvent{
            OtaEvent::Type::StartInstall,
            std::move(bundlePath),
            ""
        });
    } catch (...) {
        machine_.clearInstallStopToken();
        throw;
    }
    machine_.clearInstallStopToken();
}

void OtaService::cancelInstall() {
    std::scoped_lock lock(installMutex_);
    reapFinishedInstallLocked(machine_.getStatus());
    if (installThread_.joinable()) {
        installThread_.request_stop();
    }
}

void OtaService::resumeAfterBoot() {
    dispatchEvent(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});
}

void OtaService::markGood() {
    dispatchEvent(OtaEvent{OtaEvent::Type::MarkGood, "", ""});
}

void OtaService::markBad() {
    dispatchEvent(OtaEvent{OtaEvent::Type::MarkBad, "", ""});
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
