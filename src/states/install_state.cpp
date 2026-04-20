#include "aegis/states/install_state.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "aegis/bundle/bundle_extractor.hpp"
#include "aegis/bundle/bundle_manifest.hpp"
#include "aegis/ota_state_machine.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

void InstallState::onEnter(OtaStateMachine& machine) {
    try {
        const auto& bundlePath = machine.getStatus().bundlePath;
        const auto& config = machine.config();
        const auto payloadSize = machine.verifier().payloadSize(bundlePath);

        machine.setProgress(OtaState::Install, "verify", 20, "Verifying bundle signature");

        auto manifest = machine.verifier().verifyBundle(bundlePath, config);
        if (manifest) {
            machine.setBundleVersion(manifest->version);
        }

        // Resolve full manifest: from CMS for signed bundles, or by extracting
        // manifest.ini only for unsigned bundles (avoids touching large payloads).
        BundleManifest fullManifest;
        if (manifest) {
            fullManifest = *manifest;
        } else {
            const auto workDir = joinPath(config.dataDirectory, "bundle-work");
            std::filesystem::remove_all(workDir);
            std::filesystem::create_directories(workDir);
            machine.setInstallPath(workDir);

            BundleExtractor extractor;
            extractor.extractEntry(bundlePath, payloadSize, "manifest.ini", workDir);
            fullManifest = machine.verifier().loadManifest(workDir, config);
        }

        const auto* rootfsImage = fullManifest.findImageBySlotClass("rootfs");
        if (!rootfsImage) {
            throw std::runtime_error("Bundle does not contain a rootfs image");
        }

        const auto targetSlotName = machine.bootControl().getInactiveSlot();
        const auto& targetSlot = config.slotByBootname(targetSlotName);
        machine.setTargetSlot(targetSlotName);
        machine.setBundleVersion(fullManifest.version);

        machine.setProgress(OtaState::Install, "install", 60, "Installing payload");

        machine.updateHandlerFor(rootfsImage->imagetype).installFromBundle(
            bundlePath,
            payloadSize,
            rootfsImage->filename,
            rootfsImage->sha256,
            targetSlot,
            joinPath(config.dataDirectory, "installer-work"));

        std::filesystem::remove_all(joinPath(config.dataDirectory, "installer-work"));

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
