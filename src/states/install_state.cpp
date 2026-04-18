#include "aegis/states/install_state.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "aegis/bundle_manifest.hpp"
#include "aegis/ota_state_machine.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

void InstallState::onEnter(OtaStateMachine& machine) {
    try {
        machine.setProgress(OtaState::Install, "verify", 20, "Verifying bundle signature");

        auto manifest = machine.verifier().verifyBundle(machine.getStatus().bundlePath,
                                                        machine.config());
        if (manifest) {
            machine.setBundleVersion(manifest->version);
        }

        machine.setProgress(OtaState::Install, "extract", 40, "Extracting bundle");

        machine.setInstallPath(machine.extractBundle(machine.getStatus().bundlePath));

        const auto fullManifest = manifest
            ? *manifest
            : machine.verifier().loadManifest(machine.getStatus().installPath, machine.config());

        machine.setProgress(OtaState::Install, "install", 60, "Verifying payloads");

        machine.verifier().verifyPayloads(fullManifest, machine.getStatus().installPath);

        const auto* rootfsImage = fullManifest.findImageBySlotClass("rootfs");
        if (!rootfsImage) {
            throw std::runtime_error("Bundle does not contain a rootfs image");
        }

        const auto targetSlotName = machine.bootControl().getInactiveSlot();
        const auto& targetSlot = machine.config().slotByBootname(targetSlotName);
        const auto payloadPath = joinPath(machine.getStatus().installPath, rootfsImage->filename);

        machine.setTargetSlot(targetSlotName);
        machine.setBundleVersion(fullManifest.version);
        machine.setProgress(OtaState::Install, "install", 70, "Installing payload");

        machine.updateHandlerFor(rootfsImage->imagetype).install(
            payloadPath,
            targetSlot,
            joinPath(machine.config().dataDirectory, "installer-work"));

        machine.bootControl().setSlotBootable(targetSlotName, true);
        machine.bootControl().setPrimarySlot(targetSlotName);

        machine.updateSlots(machine.getStatus().bootedSlot, targetSlotName);
        machine.setProgress(OtaState::Install, "activate", 90, "Activating target slot");

        machine.transitionTo(std::make_unique<RebootState>());
    } catch (const std::exception& e) {
        machine.transitionTo(std::make_unique<FailureState>(e.what()));
    }
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
