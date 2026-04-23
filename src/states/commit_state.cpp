#include "aegis/states/commit_state.hpp"

#include <memory>

#include "aegis/core/ota_state_machine.hpp"
namespace aegis {

void CommitState::onEnter(OtaStateMachine& machine) {
    machine.clearLastError();
    machine.setProgress(OtaState::Commit, "commit", 100,
                        "Booted into expected slot");
    // report to gcs
    machine.transitionToIdle();
}

void CommitState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::MarkGood: {
        const auto slot = machine.bootControl().getBootedSlot();
        machine.bootControl().markGood(slot);
        machine.updateSlots(slot, slot);
        machine.clearWorkflowData();
        if (auto* gcs = machine.gcsClient()) {
            OtaStatus snapshot = machine.getStatus();
            snapshot.message = "OTA complete";
            gcs->reportStatus(snapshot);
        }
        machine.transitionToIdle();
        return;
    }

    case OtaEvent::Type::MarkBad: {
        const auto slot = machine.bootControl().getBootedSlot();
        machine.bootControl().markBad(slot);
        machine.transitionToFailure("Marked current slot bad");
        return;
    }

    case OtaEvent::Type::Reset:
        machine.transitionToIdle();
        return;

    default:
        return;
    }
}

}  // namespace aegis
