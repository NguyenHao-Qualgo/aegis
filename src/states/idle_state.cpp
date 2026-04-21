#include "aegis/states/idle_state.hpp"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

#include "aegis/core/ota_event.hpp"
#include "aegis/core/ota_state_machine.hpp"
#include "aegis/states/download_state.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

static constexpr auto kPollInterval = std::chrono::seconds(30);

IdleState::~IdleState() {
    stopAndJoinPollThread();
}

void IdleState::stopAndJoinPollThread() {
    stopPolling_ = true;
    pollCv_.notify_all();
    if (pollThread_.joinable()) {
        pollThread_.join();
    }
}

void IdleState::onEnter(OtaStateMachine& machine) {
    machine.setIdle("Waiting for update");
    logDebug(machine.getStatus().message);

    if (machine.gcsClient()) {
        machine.gcsClient()->reportStatus(machine.getStatus());
        stopPolling_ = false;
        pollThread_ = std::thread([this, &machine]() { pollLoop(machine); });
    }
}

void IdleState::onExit(OtaStateMachine&) {
    stopAndJoinPollThread();
}

void IdleState::pollLoop(OtaStateMachine& machine) {
    while (true) {
        std::unique_lock<std::mutex> lock(pollMutex_);
        pollCv_.wait_for(lock, kPollInterval, [this]() { return stopPolling_.load(); });
        lock.unlock();

        if (stopPolling_) break;

        auto update = machine.gcsClient()->checkForUpdate();
        if (update) {
            stopPolling_ = true;
            OtaEvent ev;
            ev.type = OtaEvent::Type::StartInstall;
            ev.bundlePath = update->bundleUrl;
            // Dispatch on a separate thread: we cannot call dispatch() directly
            // here because dispatch() would eventually call onExit() on this
            // IdleState, which would try to join pollThread_ — deadlock.
            std::thread([&machine, ev]() { machine.dispatch(ev); }).detach();
            break;
        }
    }
}

void IdleState::handle(OtaStateMachine& machine, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::StartInstall:
        try {
            if (event.bundlePath.empty()) {
                throw std::runtime_error("StartInstall requires a bundle path");
            }
            stopPolling_ = true;
            pollCv_.notify_all();
            machine.setBundlePath(event.bundlePath);
            machine.clearLastError();
            machine.updateSlots(machine.bootControl().getBootedSlot(),
                                machine.bootControl().getPrimarySlot());
            const auto status = machine.getStatus();
            if (status.bootedSlot != status.primarySlot) {
                logWarn("Oops, someone has manually set active slot or service restart at Reboot state");
            }
            machine.transitionTo(std::make_unique<DownloadState>());
        } catch (const std::exception& e) {
            machine.transitionTo(std::make_unique<FailureState>(e.what()));
        }
        return;

    case OtaEvent::Type::ResumeAfterBoot:
    case OtaEvent::Type::MarkGood:
    case OtaEvent::Type::MarkBad:
    case OtaEvent::Type::Reset:
        return;
    }
}

}  // namespace aegis
