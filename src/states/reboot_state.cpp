#include "aegis/states/reboot_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/core/ota_state_machine.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

RebootState::RebootState(bool restored)
    : restored_(restored) {
}

void RebootState::onEnter(OtaStateMachine& machine) {
    if (restored_) {
        const auto status = machine.getStatus();
        if (status.targetSlot) {
            LOG_I("Restored Reboot state for target slot " + *status.targetSlot +
                  " waiting for ResumeAfterBoot");
        } else {
            LOG_W("Restored Reboot state without a persisted target slot");
        }
        return;
    }

    if (!machine.getStatus().targetSlot) {
        const auto target = machine.bootControl().getPrimarySlot();
        machine.setTargetSlot(target);
        LOG_W("Reboot target slot was missing; falling back to primary slot {}", target);
    }

    machine.progress().complete(ProgressPhase::RebootReady);
    // reboot
    LOG_I("Rebooting system now...");
}

void RebootState::onExit(OtaStateMachine& machine) {
    (void)machine;
}

void RebootState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::Reset:
        machine.transitionToIdle();
        return;

    case OtaEvent::Type::ResumeAfterBoot: {
        LOG_I("System resumed after reboot, transitioning to Commit");
        machine.transitionToCommit();
        return;
    }

    default:
        return;
    }
}

}  // namespace aegis
