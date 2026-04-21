#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "aegis/core/ota_context.hpp"
#include "aegis/core/ota_event.hpp"
#include "aegis/core/ota_state.hpp"
#include "aegis/config/state_store.hpp"
#include "aegis/core/types.hpp"

namespace aegis {

class OtaStateMachine {
public:
    // When initialState is null, state is restored from the persisted StateStore.
    OtaStateMachine(OtaContext context,
                    StateStore stateStore,
                    std::unique_ptr<IOtaState> initialState = nullptr);

    // State machine
    void dispatch(const OtaEvent& event);
    void transitionTo(std::unique_ptr<IOtaState> next);
    OtaStatus getStatus() const;
    void setStatusChangedCallback(std::function<void(const OtaStatus&)> cb);

    // Terminal status transitions (persist + notify)
    void setIdle(const std::string& message = "");
    void setFailure(const std::string& error, const std::string& message = "OTA failed");

    // In-progress status update (persist + notify)
    void setProgress(OtaState state, std::string op, int progress, std::string message);

    // Workflow data setters — in-memory only, persisted on next setProgress/setIdle/setFailure
    void setBundlePath(std::string path);
    void setInstallPath(std::string path);
    void clearInstallPath();
    void setTargetSlot(std::optional<std::string> slot);
    void setBundleVersion(std::string version);
    void setLastError(std::string error);
    void updateSlots(std::string booted, std::string primary);
    void clearLastError();
    void clearWorkflowData();  // clears targetSlot, bundleVersion, lastError

    // Context service accessors (forwarded from OtaContext)
    const OtaConfig& config() const;
    const IBootControl& bootControl() const;

    IGcsClient* gcsClient() const;

    // Higher-level domain operations
    std::string downloadBundle(const std::string& url);
    void markActive(const std::string& slot);
    void discardPendingRebootState();

private:
    void init(std::unique_ptr<IOtaState> initialState);
    void save();

    OtaContext context_;
    StateStore stateStore_;
    OtaStatus status_;

    std::unique_ptr<IOtaState> state_;
    mutable std::mutex mutex_;
    std::function<void(const OtaStatus&)> onStatusChanged_;
};

}  // namespace aegis
