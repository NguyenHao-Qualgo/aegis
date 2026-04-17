#include "aegis/states/failure_state.hpp"

#include <memory>
#include <utility>

#include "aegis/ota_context.hpp"
#include "aegis/states/idle_state.hpp"

namespace aegis {

FailureState::FailureState(std::string error)
    : error_(std::move(error)) {
}

void FailureState::onEnter(OtaContext& ctx) {
    ctx.status_.state = OtaState::Failure;
    ctx.status_.operation = "failure";
    ctx.status_.progress = 0;
    ctx.status_.message = "OTA failed";
    ctx.status_.lastError = error_;
    ctx.save();
}

void FailureState::handle(OtaContext& ctx, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::Reset:
    case OtaEvent::Type::MarkBad:
    case OtaEvent::Type::ResumeAfterBoot:
        ctx.transitionTo(std::make_unique<IdleState>());
        return;

    default:
        return;
    }
}

}  // namespace aegis