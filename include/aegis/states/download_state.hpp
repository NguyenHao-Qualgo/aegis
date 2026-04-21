#pragma once

#include "aegis/core/ota_state.hpp"

namespace aegis {

class DownloadState : public IOtaState {
public:
    const char* name() const override { return "Download"; }
    void onEnter(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;
};

}  // namespace aegis
