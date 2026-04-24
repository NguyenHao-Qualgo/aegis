#include "aegis/states/failure_state.hpp"

#include <memory>
#include <utility>

#include "aegis/core/ota_state_machine.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

FailureState::FailureState(std::string error)
    : error_(std::move(error)) {
}

void FailureState::onEnter(OtaStateMachine& machine) {
    machine.setFailure(error_);
    LOG_E("OTA failure: " + error_);
    if (auto* gcs = machine.gcsClient()) {
        gcs->reportStatus(machine.getStatus());
    }
    machine.transitionToIdle();
}

void FailureState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::Reset:
    case OtaEvent::Type::MarkBad:
    case OtaEvent::Type::ResumeAfterBoot:
        machine.transitionToIdle();
        return;

    default:
        return;
    }
}

}  // namespace aegis
