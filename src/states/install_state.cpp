#include "aegis/states/install_state.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "aegis/core/ota_state_machine.hpp"
#include "aegis/common/util.hpp"
#include "aegis/installer/installer.hpp"

namespace aegis {

void InstallState::onEnter(OtaStateMachine& machine) {
    InstallOptions options;
    if (!machine.bootControl().getBootedSlot().empty()) {
        options.target_slot = machine.bootControl().getBootedSlot() == "A" ? "B" : "A";
        machine.setTargetSlot(options.target_slot);
    }
    options.config = machine.config();
    options.image_path = machine.getStatus().bundlePath;
    PackageInstaller installer(options);
    try {
        installer.install(machine, machine.installStopToken());
    } catch (const aegis::Error &error) {
        LOG_E("installation failed: {}", std::string(error.what()));
        machine.transitionToFailure(error.what());
        return;
    }
    machine.setProgress(OtaState::Install, "activate", 99, "Activating target slot");
    machine.bootControl().setSlotBootable(options.target_slot , true);
    machine.bootControl().setPrimarySlot(options.target_slot);
    machine.updateSlots(machine.bootControl().getBootedSlot(), options.target_slot);
    machine.transitionToReboot();
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
