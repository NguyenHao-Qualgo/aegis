#pragma once

#include "aegis/ota_state.hpp"

namespace aegis {

class CommitState : public IOtaState {
public:
    const char* name() const override { return "Commit"; }
    void onEnter(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;
};

}  // namespace aegis
