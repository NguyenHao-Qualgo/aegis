#include "aegis/ota_context.hpp"

#include <filesystem>
#include <stdexcept>

#include "aegis/bundle_extractor.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

OtaContext::OtaContext(OtaConfig config,
                       BootControl bootControl,
                       BundleVerifier verifier,
                       std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
                       StateStore stateStore)
    : config_(std::move(config)),
      bootControl_(std::move(bootControl)),
      verifier_(std::move(verifier)),
      updateHandlers_(std::move(updateHandlers)),
      stateStore_(std::move(stateStore)) {
    status_ = stateStore_.load();
    status_.bootedSlot = bootControl_.getBootedSlot();
    status_.primarySlot = bootControl_.getPrimarySlot();
    transitionTo(std::make_unique<IdleState>());
}

void OtaContext::dispatch(const OtaEvent& event) {
    std::scoped_lock lock(mutex_);
    if (!state_) {
        throw std::runtime_error("OTA state machine is not initialized");
    }
    state_->handle(*this, event);
}

void OtaContext::transitionTo(std::unique_ptr<IOtaState> next) {
    if (!next) {
        throw std::runtime_error("Cannot transition to null OTA state");
    }

    if (state_) {
        state_->onExit(*this);
    }

    state_ = std::move(next);
    state_->onEnter(*this);
}

OtaStatus OtaContext::getStatus() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void OtaContext::save() {
    stateStore_.save(status_);
    notifyStatusChanged();
}

void OtaContext::setIdle(const std::string& message) {
    status_.state = OtaState::Idle;
    status_.operation = "idle";
    status_.progress = 0;
    status_.message = message;
    save();
}

void OtaContext::setFailure(const std::string& error, const std::string& message) {
    status_.state = OtaState::Failure;
    status_.operation = "failure";
    status_.progress = 0;
    status_.message = message;
    status_.lastError = error;
    save();
}

const IUpdateHandler& OtaContext::updateHandlerFor(const std::string& imageType) const {
    for (const auto& handler : updateHandlers_) {
        if (handler->supportsImageType(imageType)) {
            return *handler;
        }
    }
    throw std::runtime_error("Unsupported payload type: " + imageType);
}

void OtaContext::ensureBootable(const std::string& slot) const {
    if (!bootControl_.isSlotBootable(slot)) {
        throw std::runtime_error("Slot is not bootable: " + slot);
    }
}

std::string OtaContext::extractBundle(const std::string& bundlePath) {
    const auto workDir = joinPath(config_.dataDirectory, "bundle-work");
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    BundleExtractor extractor;
    extractor.extract(bundlePath, workDir, verifier_.payloadSize(bundlePath));
    return workDir;
}

std::string OtaContext::getPrimary() const {
    return bootControl_.getPrimarySlot();
}

std::string OtaContext::getBooted() const {
    return bootControl_.getBootedSlot();
}

void OtaContext::markActive(const std::string& slot) {
    ensureBootable(slot);
    bootControl_.setPrimarySlot(slot);

    status_.primarySlot = slot;
    status_.bootedSlot = bootControl_.getBootedSlot();
    status_.lastError.clear();
    status_.message = "Set primary slot";
    setIdle();
}

void OtaContext::discardPendingRebootState() {
    status_.targetSlot.reset();
    status_.bundleVersion.clear();
    status_.lastError.clear();
    status_.message.clear();
    status_.state = OtaState::Idle;
    status_.operation = "idle";
    status_.progress = 0;
    save();
}

void OtaContext::setStatusChangedCallback(std::function<void(const OtaStatus&)> cb) {
    std::scoped_lock lock(mutex_);
    onStatusChanged_ = std::move(cb);
}

void OtaContext::notifyStatusChanged() {
    std::function<void(const OtaStatus&)> cb;
    OtaStatus snapshot;
    {
        std::scoped_lock lock(mutex_);
        cb = onStatusChanged_;
        snapshot = status_;
    }
    if (cb) {
        cb(snapshot);
    }
}

}  // namespace aegis