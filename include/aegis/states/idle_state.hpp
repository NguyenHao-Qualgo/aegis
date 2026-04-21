#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "aegis/core/ota_state.hpp"

namespace aegis {

class IdleState : public IOtaState {
public:
    ~IdleState();

    const char* name() const override { return "Idle"; }
    void onEnter(OtaStateMachine& machine) override;
    void onExit(OtaStateMachine& machine) override;
    void handle(OtaStateMachine& machine, const OtaEvent& event) override;

private:
    void pollLoop(OtaStateMachine& machine);
    void stopAndJoinPollThread();

    std::atomic<bool> stopPolling_{false};
    std::mutex pollMutex_;
    std::condition_variable pollCv_;
    std::thread pollThread_;
};

}  // namespace aegis
