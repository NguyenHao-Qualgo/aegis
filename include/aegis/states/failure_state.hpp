#pragma once

#include <string>

#include "aegis/ota_state.hpp"

namespace aegis {

class FailureState : public IOtaState {
public:
    explicit FailureState(std::string error);

    const char* name() const override { return "Failure"; }
    void onEnter(OtaContext& ctx) override;
    void handle(OtaContext& ctx, const OtaEvent& event) override;

private:
    std::string error_;
};

}  // namespace aegis