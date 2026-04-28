#include "aegis/core/ota_state_machine.hpp"

#include <filesystem>
#include <stdexcept>

#include "aegis/common/command_runner.hpp"
#include "aegis/states/commit_state.hpp"
#include "aegis/states/download_state.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/states/install_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

std::unique_ptr<IOtaState> OtaStateMachine::stateFromPersisted(const OtaStatus& status) {
    switch (status.state) {
    case OtaState::Reboot:
        LOG_I("Resuming persisted Reboot state, waiting for ResumeAfterBoot");
        return std::make_unique<RebootState>(true);
    case OtaState::Commit:
        LOG_I("Resuming persisted Commit state");
        return std::make_unique<CommitState>();
    case OtaState::Failure:
        LOG_W("Resuming persisted Failure state: {}", status.lastError);
        return std::make_unique<FailureState>(status.lastError);
    default:
        return std::make_unique<IdleState>();
    }
}

OtaStateMachine::OtaStateMachine(OtaContext context,
                                   StateStore stateStore,
                                   std::unique_ptr<IOtaState> initialState)
    : context_(std::move(context)),
      stateStore_(std::move(stateStore)),
      progress_(*this) {
    OtaStatus persisted = stateStore_.load();
    init(initialState ? std::move(initialState) : stateFromPersisted(persisted));
}

void OtaStateMachine::init(std::unique_ptr<IOtaState> initialState) {
    status_ = stateStore_.load();
    status_.bootedSlot = context_.bootControl().getBootedSlot();
    status_.primarySlot = context_.bootControl().getPrimarySlot();
    state_ = std::move(initialState);
    state_->onEnter(*this);
}

void OtaStateMachine::dispatch(const OtaEvent& event) {
    std::unique_ptr<IOtaState> current;
    {
        std::scoped_lock lock(mutex_);
        current = std::move(state_);
    }

    current->handle(*this, event);

    {
        std::scoped_lock lock(mutex_);
        if (state_ == nullptr) {
            state_ = std::move(current);
        } else {
            current->onExit(*this);
        }
    }
}

void OtaStateMachine::transitionTo(std::unique_ptr<IOtaState> next) {
    if (!next) {
        throw std::runtime_error("Cannot transition to null OTA state");
    }

    std::unique_ptr<IOtaState> old;
    {
        std::scoped_lock lock(mutex_);
        old = std::move(state_);
        state_ = std::move(next);
    }

    if (old) {
        old->onExit(*this);
    }

    state_->onEnter(*this);
}

void OtaStateMachine::transitionToIdle() {
    transitionTo(std::make_unique<IdleState>());
}

void OtaStateMachine::transitionToDownload() {
    transitionTo(std::make_unique<DownloadState>());
}

void OtaStateMachine::transitionToInstall() {
    transitionTo(std::make_unique<InstallState>());
}

void OtaStateMachine::transitionToReboot() {
    transitionTo(std::make_unique<RebootState>());
}

void OtaStateMachine::transitionToCommit() {
    transitionTo(std::make_unique<CommitState>());
}

void OtaStateMachine::transitionToFailure(std::string error) {
    transitionTo(std::make_unique<FailureState>(std::move(error)));
}

OtaStatus OtaStateMachine::getStatus() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void OtaStateMachine::save(bool save_state) {
    OtaStatus snapshot;
    std::function<void(const OtaStatus&)> cb;

    {
        std::scoped_lock lock(mutex_);
        if (save_state) {
            stateStore_.save(status_);
        }
        snapshot = status_;
        cb = onStatusChanged_;
    }

    if (cb) {
        cb(snapshot);
    }
}

void OtaStateMachine::setProgress(OtaState state, std::string op, int progress, std::string message, bool save_state) {
    status_.state = state;
    status_.operation = std::move(op);
    status_.progress = progress;
    status_.message = std::move(message);
    save(save_state);
}

void OtaStateMachine::setIdle(const std::string& message) {
    status_.state = OtaState::Idle;
    status_.operation = "idle";
    status_.progress = 0;
    status_.message = message;
    status_.targetSlot.reset();
    save();
}

void OtaStateMachine::setFailure(const std::string& error, const std::string& message) {
    status_.state = OtaState::Failure;
    status_.operation = "failure";
    status_.progress = 0;
    status_.message = message;
    status_.lastError = error;
    save();
}

void OtaStateMachine::setBundlePath(std::string path) {
    status_.bundlePath = std::move(path);
}

void OtaStateMachine::setInstallPath(std::string path) {
    status_.installPath = std::move(path);
}

void OtaStateMachine::clearInstallPath() {
    status_.installPath.clear();
}

void OtaStateMachine::setTargetSlot(std::optional<std::string> slot) {
    status_.targetSlot = std::move(slot);
}

void OtaStateMachine::setBundleVersion(std::string version) {
    status_.bundleVersion = std::move(version);
}

void OtaStateMachine::setLastError(std::string error) {
    status_.lastError = std::move(error);
}

void OtaStateMachine::updateSlots(std::string booted, std::string primary) {
    status_.bootedSlot = std::move(booted);
    status_.primarySlot = std::move(primary);
}

void OtaStateMachine::clearLastError() {
    status_.lastError.clear();
}

void OtaStateMachine::clearWorkflowData() {
    status_.targetSlot.reset();
    status_.bundleVersion.clear();
    status_.lastError.clear();
}

void OtaStateMachine::setInstallStopToken(std::stop_token stop) {
    std::scoped_lock lock(mutex_);
    installStopToken_ = stop;
}

std::stop_token OtaStateMachine::installStopToken() const {
    std::scoped_lock lock(mutex_);
    return installStopToken_;
}

void OtaStateMachine::clearInstallStopToken() {
    std::scoped_lock lock(mutex_);
    installStopToken_ = {};
}

void OtaStateMachine::setStatusChangedCallback(std::function<void(const OtaStatus&)> cb) {
    std::scoped_lock lock(mutex_);
    onStatusChanged_ = std::move(cb);
}

const OtaConfig& OtaStateMachine::config() const {
    return context_.config();
}

const IBootControl& OtaStateMachine::bootControl() const {
    return context_.bootControl();
}

IGcsClient* OtaStateMachine::gcsClient() const {
    return context_.gcsClient();
}

std::string OtaStateMachine::downloadBundle(const std::string& url) {
    if (url.find_first_of("'\"\\;&|`$<>!\n\r") != std::string::npos) {
        throw std::runtime_error("Unsafe characters in bundle URL");
    }
    const auto destPath = joinPath(context_.config().data_directory, "bundle-download");
    CommandRunner runner;
    runner.runOrThrow("curl -fsSL --max-time 300 -o '" + destPath + "' '" + url + "'");
    return destPath;
}

void OtaStateMachine::markActive(const std::string& slot) {
    if (!context_.bootControl().isSlotBootable(slot)) {
        throw std::runtime_error("Slot is not bootable: " + slot);
    }
    context_.bootControl().setPrimarySlot(slot);
    updateSlots(context_.bootControl().getBootedSlot(), slot);
    clearLastError();
    setIdle("Set primary slot");
}

void OtaStateMachine::discardPendingRebootState() {
    clearWorkflowData();
    setIdle();
}

ProgressReporter& OtaStateMachine::progress() noexcept {
    return progress_;
}

}  // namespace aegis
