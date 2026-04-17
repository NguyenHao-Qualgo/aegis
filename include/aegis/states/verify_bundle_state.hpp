#pragma once

#include "aegis/ota_state.hpp"

namespace aegis {

class VerifyBundleState : public IOtaState {
public:
    const char* name() const override { return "VerifyBundle"; }
    void onEnter(OtaContext& ctx) override;
    void handle(OtaContext& ctx, const OtaEvent& event) override;
};

}  // namespace aegis