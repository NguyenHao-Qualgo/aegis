#include "aegis/states/install_state.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "aegis/ota_state_machine.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/util.hpp"
#include "aegis/installer.hpp"

namespace aegis {

void InstallState::onEnter(OtaStateMachine& machine) {
    InstallOptions options;
    if (!machine.bootControl().getBootedSlot().empty()) {
        options.target_slot = machine.bootControl().getBootedSlot() == "A" ? "B" : "A";
    }
    options.config = machine.config();
    options.image_path = machine.getStatus().bundlePath;
    PackageInstaller installer(options);
    try {
        installer.install();
    } catch (const aegis::Error &error) {
        logError("installation failed: " + std::string(error.what()));
        machine.transitionTo(std::make_unique<FailureState>(error.what()));
        return;
    }
    machine.transitionTo(std::make_unique<RebootState>());
}

void InstallState::onExit(OtaStateMachine& machine) {
    if (!machine.getStatus().installPath.empty()) {
        std::filesystem::remove_all(machine.getStatus().installPath);
        machine.clearInstallPath();
    }
}

void InstallState::handle(OtaStateMachine&, const OtaEvent&) {
}

}  // namespace aegis
