#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "aegis/ota_state.hpp"

namespace aegis {

class IdleState : public IOtaState {
public:
    const char* name() const override { return "Idle"; }
    void onEnter(OtaContext& ctx) override;
    void onExit(OtaContext& ctx) override;
    void handle(OtaContext& ctx, const OtaEvent& event) override;

private:
    void pollLoop(OtaContext& ctx);

    std::atomic<bool> stopPolling_{false};
    std::mutex pollMutex_;
    std::condition_variable pollCv_;
    std::thread pollThread_;
};

}  // namespace aegis
