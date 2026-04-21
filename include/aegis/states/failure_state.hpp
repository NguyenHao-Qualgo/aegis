#pragma once

#include <string>

#include "aegis/core/ota_state.hpp"

namespace aegis {

class FailureState : public IOtaState {
public:
    explicit FailureState(std::string error);

    const char* name() const override { return "Failure"; }
    void onEnter(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;

private:
    std::string error_;
};

}  // namespace aegis
