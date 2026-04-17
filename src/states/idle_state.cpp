
#include "aegis/states/idle_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/ota_context.hpp"
#include "aegis/states/sync_state.hpp"

namespace aegis {

void IdleState::onEnter(OtaContext& ctx) {
    ctx.status_.state = OtaState::Idle;
    ctx.status_.operation = "idle";
    ctx.status_.progress = 0;
    ctx.save();
}

void IdleState::handle(OtaContext& ctx, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::StartInstall:
        if (event.bundlePath.empty()) {
            throw std::runtime_error("StartInstall requires a bundle path");
        }
        ctx.status_.bundlePath = event.bundlePath;
        ctx.status_.lastError.clear();
        ctx.transitionTo(std::make_unique<SyncState>());
        return;

    case OtaEvent::Type::ResumeAfterBoot:
    case OtaEvent::Type::MarkGood:
    case OtaEvent::Type::MarkBad:
    case OtaEvent::Type::Reset:
        return;
    }
}

}  // namespace aegis