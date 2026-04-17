#include "aegis/states/install_state.hpp"

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
        ctx.status_.operation = "install";
        ctx.status_.progress = 50;
        ctx.status_.message = "Verifying payloads";
        ctx.save();

        auto preManifest = ctx.verifier_.verifyBundle(ctx.status_.bundlePath, ctx.config_);
        const auto manifest = preManifest
            ? *preManifest
            : ctx.verifier_.loadManifest(ctx.status_.installPath, ctx.config_);

        ctx.verifier_.verifyPayloads(manifest, ctx.status_.installPath);

        const auto* rootfsImage = manifest.findImageBySlotClass("rootfs");
        if (!rootfsImage) {
            throw std::runtime_error("Bundle does not contain a rootfs image");
        }

        const auto targetSlotName = ctx.bootControl_.getInactiveSlot();
        const auto& targetSlot = ctx.config_.slotByBootname(targetSlotName);
        const auto payloadPath = joinPath(ctx.status_.installPath, rootfsImage->filename);

        ctx.status_.targetSlot = targetSlotName;
        ctx.status_.bundleVersion = manifest.version;
        ctx.status_.progress = 70;
        ctx.status_.message = "Installing payload";
        ctx.save();

        ctx.updateHandlerFor(rootfsImage->imagetype).install(
            payloadPath,
            targetSlot,
            joinPath(ctx.config_.dataDirectory, "installer-work"));

        ctx.status_.operation = "activate";
        ctx.status_.progress = 90;
        ctx.status_.message = "Activating target slot";
        ctx.bootControl_.setSlotBootable(targetSlotName, true);
        ctx.bootControl_.setPrimarySlot(targetSlotName);
        ctx.status_.primarySlot = targetSlotName;
        ctx.save();

        ctx.transitionTo(std::make_unique<RebootState>());
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

void InstallState::handle(OtaContext&, const OtaEvent&) {
}

}  // namespace aegis