#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/command_runner.hpp"
#include "aegis/state_store.hpp"
#include "aegis/types.hpp"
#include "aegis/update_handler.hpp"

namespace aegis {

class OtaService {
public:
    OtaService(OtaConfig config,
               BootControl bootControl,
               BundleVerifier verifier,
               std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
               StateStore stateStore,
               CommandRunner runner);

    OtaStatus getStatus() const;
    void startInstall(const std::string& bundlePath);
    void markGood();
    void markBad();
    void markActive(const std::string& slot);
    std::string getPrimary() const;
    std::string getBooted() const;
    void resumeAfterBoot();

private:
    void install(const std::string& bundlePath);
    void finishInstall();
    void failInstall(const std::string& message);
    void setState(OtaState state, const std::string& operation, int progress, const std::string& message);
    void save();
    std::string extractBundle(const std::string& bundlePath);
    void ensureBootable(const std::string& slot) const;
    const IUpdateHandler& updateHandlerFor(const std::string& imageType) const;

    OtaConfig config_;
    BootControl bootControl_;
    BundleVerifier verifier_;
    std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers_;
    StateStore stateStore_;
    CommandRunner runner_;

    mutable std::mutex mutex_;
    OtaStatus status_;
    bool installInProgress_{false};
};

}  // namespace aegis
