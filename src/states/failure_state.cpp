#include "aegis/states/failure_state.hpp"

#include <memory>
#include <utility>

#include "aegis/ota_state_machine.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/util.hpp"

namespace aegis {

FailureState::FailureState(std::string error)
    : error_(std::move(error)) {
}

void FailureState::onEnter(OtaStateMachine& machine) {
    machine.setFailure(error_);
    logError("OTA failure: " + error_);
    if (auto* gcs = machine.gcsClient()) {
        gcs->reportStatus(machine.getStatus());
    }
}

void FailureState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::Reset:
    case OtaEvent::Type::MarkBad:
    case OtaEvent::Type::ResumeAfterBoot:
        machine.transitionTo(std::make_unique<IdleState>());
        return;

    default:
        return;
    }
}

}  // namespace aegis
