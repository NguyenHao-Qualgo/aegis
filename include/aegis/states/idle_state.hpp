#pragma once

#include "aegis/ota_state.hpp"

namespace aegis {

class IdleState : public IOtaState {
public:
    const char* name() const override { return "Idle"; }
    void onEnter(OtaContext& ctx) override;
    void handle(OtaContext& ctx, const OtaEvent& event) override;
};

}  //
