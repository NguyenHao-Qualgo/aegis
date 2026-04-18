#include "aegis/states/reboot_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/ota_context.hpp"
#include "aegis/states/commit_state.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

void RebootState::onEnter(OtaContext& ctx) {
    ctx.status_.state = OtaState::Reboot;
    ctx.status_.operation = "reboot";
    ctx.status_.progress = 100;
    ctx.status_.message = "Ready to reboot";
    ctx.save();
}

void RebootState::onExit(OtaContext& ctx) {
    ctx.status_.targetSlot.reset();
    ctx.status_.bundleVersion.clear();
    ctx.status_.lastError.clear();
}

void RebootState::handle(OtaContext& ctx, const OtaEvent& event) {
    if (event.type != OtaEvent::Type::ResumeAfterBoot) {
        return;
    }

    try {
        const auto booted = ctx.getBooted();
        const auto primary = ctx.getPrimary();

        ctx.status_.bootedSlot = booted;
        ctx.status_.primarySlot = primary;

        if (!ctx.status_.targetSlot) {
            throw std::runtime_error("Missing target slot while resuming after reboot");
        }

        if (booted == *ctx.status_.targetSlot) {
            logInfo("Booted into expected slot " + booted + " — transitioning to Commit");
            ctx.transitionTo(std::make_unique<CommitState>());
            return;
        }

        logWarn("Slot mismatch: expected " + *ctx.status_.targetSlot +
                " but booted into " + booted + " (possible watchdog rollback)");

        // Rollback: point primary slot back to the slot we actually booted
        // so the next reboot stays on the working slot instead of retrying
        // the failed target.
        try {
            ctx.bootControl_->setPrimarySlot(booted);
            ctx.status_.primarySlot = booted;
            logInfo("Primary slot reset to " + booted);
        } catch (const std::exception& e) {
            logWarn("Failed to reset primary slot to " + booted + ": " + e.what());
        }

        ctx.transitionTo(std::make_unique<FailureState>(
            "Booted slot does not match expected target"));
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

}  // namespace aegis