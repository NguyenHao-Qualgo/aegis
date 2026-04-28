#include "aegis/states/commit_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/common/util.hpp"
#include "aegis/core/ota_state_machine.hpp"
namespace aegis {

void CommitState::onEnter(OtaStateMachine& machine) {
    try {
        machine.progress().begin(ProgressPhase::CommitCheck);
        const auto booted = machine.bootControl().getBootedSlot();
        const auto primary = machine.bootControl().getPrimarySlot();
        machine.updateSlots(booted, primary);

        const auto status = machine.getStatus();
        if (!status.targetSlot) {
            throw std::runtime_error("Missing target slot while committing reboot");
        }

        if (booted != *status.targetSlot) {
            LOG_W("Slot mismatch: expected {} but booted into {} (possible watchdog rollback)", *status.targetSlot, booted);

            try {
                machine.bootControl().setPrimarySlot(booted);
                machine.updateSlots(booted, booted);
                LOG_I("Primary slot reset to {}", booted);
            } catch (const std::exception& e) {
                LOG_W("Failed to reset primary slot to {}: {}", booted, e.what());
            }
            if (auto* gcs = machine.gcsClient()) {
                OtaStatus snapshot = machine.getStatus();
                snapshot.message = "OTA failed due to slot mismatch after reboot";
                gcs->reportStatus(snapshot);
            }

            machine.transitionToFailure("Booted slot does not match expected target");
            return;
        }

        machine.clearLastError();
        machine.progress().complete(ProgressPhase::CommitDone);
        if (auto* gcs = machine.gcsClient()) {
            OtaStatus snapshot = machine.getStatus();
            snapshot.message = "OTA complete";
            gcs->reportStatus(snapshot);
        }
        machine.clearWorkflowData();
        machine.transitionToIdle();
    } catch (const std::exception& e) {
        machine.transitionToFailure(e.what());
    }
}

void CommitState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::MarkGood: {
        const auto slot = machine.bootControl().getBootedSlot();
        machine.bootControl().markGood(slot);
        machine.updateSlots(slot, slot);
        machine.clearWorkflowData();
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
