#pragma once

#include "aegis/ota_state.hpp"

namespace aegis {

class DownloadState : public IOtaState {
public:
    const char* name() const override { return "Download"; }
    void onEnter(OtaContext& ctx) override;
    void handle(OtaContext& ctx, const OtaEvent& event) override;
};

}  // namespace aegis
