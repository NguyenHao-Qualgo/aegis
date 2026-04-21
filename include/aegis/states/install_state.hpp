#pragma once

#include "aegis/core/ota_state.hpp"

namespace aegis {

class InstallState : public IOtaState {
public:
    const char* name() const override { return "Install"; }
    void onEnter(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;
    void onExit(OtaStateMachine& machine) override;
};

}  // namespace aegis
