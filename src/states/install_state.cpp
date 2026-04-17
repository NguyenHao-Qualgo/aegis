#include "aegis/states/install_state.hpp"

#include <filesystem>
#include <memory>
#include <stdexcept>

#include "aegis/bundle_manifest.hpp"
#include "aegis/ota_context.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

void InstallState::onEnter(OtaContext& ctx) {
    try {
        ctx.status_.state = OtaState::Install;
        ctx.status_.operation = "verify";
        ctx.status_.progress = 20;
        ctx.status_.message = "Verifying bundle signature";
        ctx.save();

        auto manifest = ctx.verifier_->verifyBundle(ctx.status_.bundlePath, ctx.config_);
        if (manifest) {
            ctx.status_.bundleVersion = manifest->version;
        }

        ctx.status_.operation = "extract";
        ctx.status_.progress = 40;
        ctx.status_.message = "Extracting bundle";
        ctx.save();

        ctx.status_.installPath = ctx.extractBundle(ctx.status_.bundlePath);

        const auto fullManifest = manifest
            ? *manifest
            : ctx.verifier_->loadManifest(ctx.status_.installPath, ctx.config_);

        ctx.status_.operation = "install";
        ctx.status_.progress = 60;
        ctx.status_.message = "Verifying payloads";
        ctx.save();

        ctx.verifier_->verifyPayloads(fullManifest, ctx.status_.installPath);

        const auto* rootfsImage = fullManifest.findImageBySlotClass("rootfs");
        if (!rootfsImage) {
            throw std::runtime_error("Bundle does not contain a rootfs image");
        }

        const auto targetSlotName = ctx.bootControl_->getInactiveSlot();
        const auto& targetSlot = ctx.config_.slotByBootname(targetSlotName);
        const auto payloadPath = joinPath(ctx.status_.installPath, rootfsImage->filename);

        ctx.status_.targetSlot = targetSlotName;
        ctx.status_.bundleVersion = fullManifest.version;
        ctx.status_.progress = 70;
        ctx.status_.message = "Installing payload";
        ctx.save();

        ctx.updateHandlerFor(rootfsImage->imagetype).install(
            payloadPath,
            targetSlot,
            joinPath(ctx.config_.dataDirectory, "installer-work"));

        ctx.bootControl_->setSlotBootable(targetSlotName, true);
        ctx.bootControl_->setPrimarySlot(targetSlotName);

        ctx.status_.operation = "activate";
        ctx.status_.progress = 90;
        ctx.status_.message = "Activating target slot";
        ctx.status_.primarySlot = targetSlotName;
        ctx.save();

        ctx.transitionTo(std::make_unique<RebootState>());
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

void InstallState::onExit(OtaContext& ctx) {
    if (!ctx.status_.installPath.empty()) {
        std::filesystem::remove_all(ctx.status_.installPath);
        ctx.status_.installPath.clear();
    }
}

void InstallState::handle(OtaContext&, const OtaEvent&) {
}

}  // namespace aegis
