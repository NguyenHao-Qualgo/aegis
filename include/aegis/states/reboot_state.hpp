#pragma once

#include "aegis/core/ota_state.hpp"

namespace aegis {

class RebootState : public IOtaState {
public:
    explicit RebootState(bool restored = false);

    const char* name() const override { return "Reboot"; }
    void onEnter(OtaStateMachine& machine) override;
    void onExit(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;

private:
    bool restored_;
};

}  // namespace aegis
