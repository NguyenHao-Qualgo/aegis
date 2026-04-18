#pragma once

#include "aegis/ota_state.hpp"

namespace aegis {

class RebootState : public IOtaState {
public:
    const char* name() const override { return "Reboot"; }
    void onEnter(OtaStateMachine& machine) override;
    void onExit(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;
};

}  // namespace aegis
