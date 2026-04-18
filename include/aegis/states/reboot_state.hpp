#pragma once

#include "aegis/ota_state.hpp"

namespace aegis {

class RebootState : public IOtaState {
public:
    const char* name() const override { return "Reboot"; }
    void onEnter(OtaContext& ctx) override;
    void onExit(OtaContext& ctx) override;
    void handle(OtaContext& ctx, const OtaEvent& event) override;
};

}  // namespace aegis