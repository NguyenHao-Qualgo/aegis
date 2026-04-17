#include "aegis/states/commit_state.hpp"

#include <memory>

#include "aegis/ota_context.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/idle_state.hpp"

namespace aegis {

void CommitState::onEnter(OtaContext& ctx) {
    ctx.status_.state = OtaState::Commit;
    ctx.status_.operation = "commit";
    ctx.status_.progress = 100;
    ctx.status_.lastError.clear();
    ctx.status_.message = "Booted into expected slot; waiting for mark-good";
    ctx.save();
}

void CommitState::handle(OtaContext& ctx, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::MarkGood: {
        const auto slot = ctx.getBooted();
        ctx.bootControl_->markGood(slot);
        ctx.status_.bootedSlot = slot;
        ctx.status_.primarySlot = slot;
        ctx.status_.targetSlot.reset();
        ctx.status_.message = "OTA complete";
        ctx.status_.lastError.clear();
        if (ctx.gcsClient_) {
            ctx.gcsClient_->reportStatus(ctx.status_);
        }
        ctx.transitionTo(std::make_unique<IdleState>());
        return;
    }

    case OtaEvent::Type::MarkBad: {
        const auto slot = ctx.getBooted();
        ctx.bootControl_->markBad(slot);
        ctx.transitionTo(std::make_unique<FailureState>("Marked current slot bad"));
        return;
    }

    case OtaEvent::Type::Reset:
        ctx.transitionTo(std::make_unique<IdleState>());
        return;

    default:
        return;
    }
}

}  // namespace aegis