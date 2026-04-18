#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/config.hpp"
#include "aegis/gcs_client.hpp"
#include "aegis/ota_event.hpp"
#include "aegis/ota_state.hpp"
#include "aegis/state_store.hpp"
#include "aegis/types.hpp"
#include "aegis/update_handler.hpp"

namespace aegis {

class OtaContext {
public:
    OtaContext(OtaConfig config,
               std::unique_ptr<IBootControl> bootControl,
               std::unique_ptr<IBundleVerifier> verifier,
               std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
               StateStore stateStore,
               std::shared_ptr<IGcsClient> gcsClient = nullptr);

    // Test constructor — starts in a custom initial state instead of IdleState.
    OtaContext(OtaConfig config,
               std::unique_ptr<IBootControl> bootControl,
               std::unique_ptr<IBundleVerifier> verifier,
               std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
               StateStore stateStore,
               std::shared_ptr<IGcsClient> gcsClient,
               std::unique_ptr<IOtaState> initialState);

    void dispatch(const OtaEvent& event);
    void transitionTo(std::unique_ptr<IOtaState> next);

    OtaStatus getStatus() const;

    void save();

    void setIdle(const std::string& message = "");
    void setFailure(const std::string& error, const std::string& message = "OTA failed");

    const IUpdateHandler& updateHandlerFor(const std::string& imageType) const;
    void ensureBootable(const std::string& slot) const;
    std::string downloadBundle(const std::string& url);
    std::string extractBundle(const std::string& bundlePath);

    std::string getPrimary() const;
    std::string getBooted() const;
    void markActive(const std::string& slot);
    void discardPendingRebootState();
    void setStatusChangedCallback(std::function<void(const OtaStatus&)> cb);
    void setState(std::unique_ptr<IOtaState> state);

    OtaConfig config_;
    std::unique_ptr<IBootControl> bootControl_;
    std::unique_ptr<IBundleVerifier> verifier_;
    std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers_;
    StateStore stateStore_;
    std::shared_ptr<IGcsClient> gcsClient_;
    OtaStatus status_;

private:
    void init(std::unique_ptr<IOtaState> initialState);

    std::function<void(const OtaStatus&)> onStatusChanged_;
    mutable std::mutex mutex_;
    std::unique_ptr<IOtaState> state_;
    bool dispatching_{false};
};

}  // namespace aegis