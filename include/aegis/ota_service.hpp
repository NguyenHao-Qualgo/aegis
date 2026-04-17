#pragma once

#include <mutex>
#include <string>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/command_runner.hpp"
#include "aegis/ext4_installer.hpp"
#include "aegis/state_store.hpp"
#include "aegis/types.hpp"

namespace aegis {

class OtaService {
public:
    OtaService(OtaConfig config,
               BootControl bootControl,
               BundleVerifier verifier,
               Ext4Installer installer,
               StateStore stateStore,
               CommandRunner runner);

    OtaStatus getStatus() const;
    void install(const std::string& bundlePath);
    void markGood();
    void markBad();
    void markActive(const std::string& slot);
    std::string getPrimary() const;
    std::string getBooted() const;
    void resumeAfterBoot();

private:
    void setState(OtaState state, const std::string& operation, int progress, const std::string& message);
    void save();
    std::string extractBundle(const std::string& bundlePath) const;
    void ensureBootable(const std::string& slot) const;

    OtaConfig config_;
    BootControl bootControl_;
    BundleVerifier verifier_;
    Ext4Installer installer_;
    StateStore stateStore_;
    CommandRunner runner_;

    mutable std::mutex mutex_;
    OtaStatus status_;
};

}  // namespace aegis
