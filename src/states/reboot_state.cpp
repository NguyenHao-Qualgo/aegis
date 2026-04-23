#include "aegis/states/reboot_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/core/ota_state_machine.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

void RebootState::onEnter(OtaStateMachine& machine) {
    machine.setTargetSlot(machine.bootControl().getInactiveSlot());
    machine.setProgress(OtaState::Reboot, "reboot", 100, "Ready to reboot");
    // reboot
    LOG_I("Rebooting system now...");
}

void RebootState::onExit(OtaStateMachine& machine) {
    machine.clearWorkflowData();
}

void RebootState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::Reset:
        machine.transitionToIdle();
        return;

    case OtaEvent::Type::ResumeAfterBoot: {
        try {
            const auto booted = machine.bootControl().getBootedSlot();
            const auto primary = machine.bootControl().getPrimarySlot();
            machine.updateSlots(booted, primary);

            const auto status = machine.getStatus();
            if (!status.targetSlot) {
                throw std::runtime_error("Missing target slot while resuming after reboot");
            }

            if (booted == *status.targetSlot) {
                LOG_I("Booted into expected slot " + booted + " — transitioning to Commit");
                machine.transitionToCommit();
                return;
            }

            LOG_W("Slot mismatch: expected " + *status.targetSlot +
                    " but booted into " + booted + " (possible watchdog rollback)");

            // Rollback: point primary slot back to the slot we actually booted
            // so the next reboot stays on the working slot instead of retrying
            // the failed target.
            try {
                machine.bootControl().setPrimarySlot(booted);
                machine.updateSlots(booted, booted);
                LOG_I("Primary slot reset to " + booted);
            } catch (const std::exception& e) {
                LOG_W("Failed to reset primary slot to " + booted + ": " + e.what());
            }

            machine.transitionToFailure("Booted slot does not match expected target");
        } catch (const std::exception& e) {
            machine.transitionToFailure(e.what());
        }
        return;
    }

    default:
        return;
    }
}

}  // namespace aegis
