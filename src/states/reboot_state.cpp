#include "aegis/states/reboot_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/ota_context.hpp"
#include "aegis/states/commit_state.hpp"
#include "aegis/states/failure_state.hpp"

namespace aegis {

void RebootState::onEnter(OtaContext& ctx) {
    ctx.status_.state = OtaState::Reboot;
    ctx.status_.operation = "reboot";
    ctx.status_.progress = 100;
    ctx.status_.message = "Ready to reboot";
    ctx.save();
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
            ctx.transitionTo(std::make_unique<CommitState>());
            return;
        }

        ctx.transitionTo(std::make_unique<FailureState>(
            "Booted slot does not match expected target"));
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

}  // namespace aegis