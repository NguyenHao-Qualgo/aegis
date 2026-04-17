#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <functional>

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
               BootControl bootControl,
               BundleVerifier verifier,
               std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
               StateStore stateStore,
               std::shared_ptr<IGcsClient> gcsClient = nullptr);

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

    OtaConfig config_;
    BootControl bootControl_;
    BundleVerifier verifier_;
    std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers_;
    StateStore stateStore_;
    std::shared_ptr<IGcsClient> gcsClient_;
    OtaStatus status_;

private:
    void notifyStatusChanged();

    std::function<void(const OtaStatus&)> onStatusChanged_;
    mutable std::mutex mutex_;
    std::unique_ptr<IOtaState> state_;
};

}  // namespace aegis