#include "aegis/ota_context.hpp"

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
        return std::make_unique<RebootState>();
    case OtaState::Commit:
        return std::make_unique<CommitState>();
    case OtaState::Failure:
        return std::make_unique<FailureState>(status.lastError);
    default:
        return std::make_unique<IdleState>();
    }
}

OtaContext::OtaContext(OtaConfig config,
                       std::unique_ptr<IBootControl> bootControl,
                       std::unique_ptr<IBundleVerifier> verifier,
                       std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
                       StateStore stateStore,
                       std::shared_ptr<IGcsClient> gcsClient)
    : config_(std::move(config)),
      bootControl_(std::move(bootControl)),
      verifier_(std::move(verifier)),
      updateHandlers_(std::move(updateHandlers)),
      stateStore_(std::move(stateStore)),
      gcsClient_(std::move(gcsClient)) {
    OtaStatus persisted = stateStore_.load();
    init(stateFromPersisted(persisted));
}

OtaContext::OtaContext(OtaConfig config,
                       std::unique_ptr<IBootControl> bootControl,
                       std::unique_ptr<IBundleVerifier> verifier,
                       std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
                       StateStore stateStore,
                       std::shared_ptr<IGcsClient> gcsClient,
                       std::unique_ptr<IOtaState> initialState)
    : config_(std::move(config)),
      bootControl_(std::move(bootControl)),
      verifier_(std::move(verifier)),
      updateHandlers_(std::move(updateHandlers)),
      stateStore_(std::move(stateStore)),
      gcsClient_(std::move(gcsClient)) {
    init(std::move(initialState));
}

void OtaContext::init(std::unique_ptr<IOtaState> initialState) {
    status_ = stateStore_.load();
    status_.bootedSlot = getBooted();
    status_.primarySlot = getPrimary();
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
void OtaContext::dispatch(const OtaEvent& event) {
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

    // Determine if a transition happened.  transitionTo() sets state_ during
    // handle() (or any onEnter() in the chain), leaving it non-null when done.
    bool transitioned;
    {
        std::scoped_lock lock(mutex_);
        transitioned = (state_ != nullptr);
    }

    // Call onExit() on the original state while dispatching_ is still true,
    // so no concurrent dispatch can run until cleanup is done.
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
void OtaContext::transitionTo(std::unique_ptr<IOtaState> next) {
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

OtaStatus OtaContext::getStatus() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void OtaContext::save() {
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
    if (!bootControl_->isSlotBootable(slot)) {
        throw std::runtime_error("Slot is not bootable: " + slot);
    }
}

std::string OtaContext::downloadBundle(const std::string& url) {
    if (url.find_first_of("'\"\\;&|`$<>!\n\r") != std::string::npos) {
        throw std::runtime_error("Unsafe characters in bundle URL");
    }
    const auto destPath = joinPath(config_.dataDirectory, "bundle-download");
    CommandRunner runner;
    runner.runOrThrow("curl -fsSL --max-time 300 -o '" + destPath + "' '" + url + "'");
    return destPath;
}

std::string OtaContext::extractBundle(const std::string& bundlePath) {
    const auto workDir = joinPath(config_.dataDirectory, "bundle-work");
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);

    BundleExtractor extractor;
    extractor.extract(bundlePath, workDir, verifier_->payloadSize(bundlePath));
    return workDir;
}

std::string OtaContext::getPrimary() const {
    return bootControl_->getPrimarySlot();
}

std::string OtaContext::getBooted() const {
    return bootControl_->getBootedSlot();
}

void OtaContext::markActive(const std::string& slot) {
    ensureBootable(slot);
    bootControl_->setPrimarySlot(slot);

    status_.primarySlot = slot;
    status_.bootedSlot = getBooted();
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

}  // namespace aegis
