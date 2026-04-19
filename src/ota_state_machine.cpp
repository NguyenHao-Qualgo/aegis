#include "aegis/ota_state_machine.hpp"

#include <filesystem>
#include <stdexcept>

#include "aegis/bundle_extractor.hpp"
#include "aegis/command_runner.hpp"
#include "aegis/states/commit_state.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

static std::unique_ptr<IOtaState> stateFromPersisted(const OtaStatus& status) {
    switch (status.state) {
    case OtaState::Reboot:
        logInfo("Resuming persisted Reboot state — waiting for ResumeAfterBoot");
        return std::make_unique<RebootState>();
    case OtaState::Commit:
        logInfo("Resuming persisted Commit state — waiting for mark-good");
        return std::make_unique<CommitState>();
    case OtaState::Failure:
        logWarn("Resuming persisted Failure state: " + status.lastError);
        return std::make_unique<FailureState>(status.lastError);
    default:
        return std::make_unique<IdleState>();
    }
}

OtaStateMachine::OtaStateMachine(OtaContext context,
                                   StateStore stateStore,
                                   std::unique_ptr<IOtaState> initialState)
    : context_(std::move(context)),
      stateStore_(std::move(stateStore)) {
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

// dispatch() runs the current state's event handler.
//
// The state is moved out of state_ before handle() is called so that
// transitionTo() (which may be called from within handle() or from any
// onEnter() in a chain of transitions) can freely set state_ to the
// next state without conflicting with dispatch's own state pointer.
//
// After handle() returns:
//   - If state_ is non-null, transitionTo() was called and the entire
//     transition chain (including all chained onEnter() calls) has already
//     completed.  We only need to call onExit() on the original state.
//   - If state_ is still null, no transition occurred; restore the original state.
void OtaStateMachine::dispatch(const OtaEvent& event) {
    std::unique_ptr<IOtaState> current;

    {
        std::scoped_lock lock(mutex_);
        if (dispatching_) {
            throw std::runtime_error("OTA dispatch called while another dispatch is in progress");
        }
        dispatching_ = true;
        current = std::move(state_);
    }

    current->handle(*this, event);

    bool transitioned;
    {
        std::scoped_lock lock(mutex_);
        transitioned = (state_ != nullptr);
    }

    if (transitioned) {
        current->onExit(*this);
    }

    {
        std::scoped_lock lock(mutex_);
        dispatching_ = false;
        if (!transitioned) {
            state_ = std::move(current);
        }
    }
}

// transitionTo() may be called from within a state's handle() or onEnter().
// It always sets the new state immediately and calls onEnter() on it.
// onExit() on the previous state is called here only when state_ is non-null
// (i.e. when called from a chained onEnter(), not from the top-level handle()
// whose original state has been moved into dispatch()'s local `current`).
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

OtaStatus OtaStateMachine::getStatus() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void OtaStateMachine::save() {
    OtaStatus snapshot;
    std::function<void(const OtaStatus&)> cb;

    {
        std::scoped_lock lock(mutex_);
        stateStore_.save(status_);
        snapshot = status_;
        cb = onStatusChanged_;
    }

    if (cb) {
        cb(snapshot);
    }
}

void OtaStateMachine::setProgress(OtaState state, std::string op, int progress, std::string message) {
    status_.state = state;
    status_.operation = std::move(op);
    status_.progress = progress;
    status_.message = std::move(message);
    save();
}

void OtaStateMachine::setIdle(const std::string& message) {
    status_.state = OtaState::Idle;
    status_.operation = "idle";
    status_.progress = 0;
    status_.message = message;
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

const IBundleVerifier& OtaStateMachine::verifier() const {
    return context_.verifier();
}

IGcsClient* OtaStateMachine::gcsClient() const {
    return context_.gcsClient();
}

const IUpdateHandler& OtaStateMachine::updateHandlerFor(const std::string& imageType) const {
    return context_.updateHandlerFor(imageType);
}

std::string OtaStateMachine::downloadBundle(const std::string& url) {
    if (url.find_first_of("'\"\\;&|`$<>!\n\r") != std::string::npos) {
        throw std::runtime_error("Unsafe characters in bundle URL");
    }
    const auto destPath = joinPath(context_.config().dataDirectory, "bundle-download");
    CommandRunner runner;
    runner.runOrThrow("curl -fsSL --max-time 300 -o '" + destPath + "' '" + url + "'");
    return destPath;
}

std::string OtaStateMachine::extractBundle(const std::string& bundlePath) {
    const auto workDir = joinPath(context_.config().dataDirectory, "bundle-work");
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    BundleExtractor extractor;
    extractor.extract(bundlePath, workDir, context_.verifier().payloadSize(bundlePath));
    return workDir;
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

}  // namespace aegis
