
#include "aegis/states/idle_state.hpp"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

#include "aegis/ota_context.hpp"
#include "aegis/ota_event.hpp"
#include "aegis/states/download_state.hpp"
#include "aegis/states/failure_state.hpp"

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

void IdleState::onEnter(OtaContext& ctx) {
    ctx.status_.state = OtaState::Idle;
    ctx.status_.operation = "idle";
    ctx.status_.progress = 0;
    ctx.status_.message = "Waiting for update";
    ctx.save();

    if (ctx.gcsClient_) {
        ctx.gcsClient_->reportStatus(ctx.status_);
        stopPolling_ = false;
        pollThread_ = std::thread([this, &ctx]() { pollLoop(ctx); });
    }
}

void IdleState::onExit(OtaContext&) {
    stopAndJoinPollThread();
}

void IdleState::pollLoop(OtaContext& ctx) {
    while (true) {
        std::unique_lock<std::mutex> lock(pollMutex_);
        pollCv_.wait_for(lock, kPollInterval, [this]() { return stopPolling_.load(); });
        lock.unlock();

        if (stopPolling_) break;

        auto update = ctx.gcsClient_->checkForUpdate();
        if (update) {
            stopPolling_ = true;
            OtaEvent ev;
            ev.type = OtaEvent::Type::StartInstall;
            ev.bundlePath = update->bundleUrl;
            // Dispatch on a separate thread: we cannot call dispatch() directly
            // here because dispatch() would eventually call onExit() on this
            // IdleState, which would try to join pollThread_ — deadlock.
            std::thread([&ctx, ev]() { ctx.dispatch(ev); }).detach();
            break;
        }
    }
}

void IdleState::handle(OtaContext& ctx, const OtaEvent& event) {
    switch (event.type) {
    case OtaEvent::Type::StartInstall:
        try {
            if (event.bundlePath.empty()) {
                throw std::runtime_error("StartInstall requires a bundle path");
            }
            stopPolling_ = true;
            pollCv_.notify_all();
            ctx.status_.bundlePath = event.bundlePath;
            ctx.status_.lastError.clear();
            ctx.status_.bootedSlot = ctx.getBooted();
            ctx.status_.primarySlot = ctx.getPrimary();
            ctx.transitionTo(std::make_unique<DownloadState>());
        } catch (const std::exception& e) {
            ctx.transitionTo(std::make_unique<FailureState>(e.what()));
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
