#include "aegis/core/progress.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "aegis/core/ota_state_machine.hpp"

namespace aegis {

const ProgressSpec& progressSpec(ProgressPhase phase) {
    static const ProgressSpec download{
        OtaState::Download,
        "download",
        0,
        29,
        "Downloading bundle",
    };

    static const ProgressSpec install_verify{
        OtaState::Install,
        "verify",
        30,
        34,
        "Verifying package signature",
    };

    static const ProgressSpec install_payload{
        OtaState::Install,
        "install",
        35,
        94,
        "Installing payload",
    };

    static const ProgressSpec install_finalize{
        OtaState::Install,
        "finalize",
        95,
        95,
        "Installation complete, finalizing",
    };

    static const ProgressSpec install_done{
        OtaState::Install,
        "done",
        100,
        100,
        "Installation completed successfully",
    };

    static const ProgressSpec reboot_ready{
        OtaState::Reboot,
        "reboot",
        100,
        100,
        "Ready to reboot",
    };

    static const ProgressSpec commit_check{
        OtaState::Commit,
        "commit",
        50,
        50,
        "Checking booted slot",
    };

    static const ProgressSpec commit_done{
        OtaState::Commit,
        "commit",
        100,
        100,
        "Booted into expected slot",
    };

    switch (phase) {
    case ProgressPhase::Download:
        return download;
    case ProgressPhase::InstallVerify:
        return install_verify;
    case ProgressPhase::InstallPayload:
        return install_payload;
    case ProgressPhase::InstallFinalize:
        return install_finalize;
    case ProgressPhase::InstallDone:
        return install_done;
    case ProgressPhase::RebootReady:
        return reboot_ready;
    case ProgressPhase::CommitCheck:
        return commit_check;
    case ProgressPhase::CommitDone:
        return commit_done;
    }

    return install_done;
}

ProgressReporter::ProgressReporter(OtaStateMachine& machine)
    : ProgressReporter(machine, Config{}) {
}

ProgressReporter::ProgressReporter(OtaStateMachine& machine, Config config)
    : machine_(machine),
      config_(config) {
}

void ProgressReporter::begin(ProgressPhase phase, std::string_view message) {
    const auto& spec = progressSpec(phase);
    publish(spec, spec.start_percent, message, true);
}

void ProgressReporter::complete(ProgressPhase phase, std::string_view message) {
    const auto& spec = progressSpec(phase);
    publish(spec, spec.end_percent, message, true);
}

void ProgressReporter::set(ProgressPhase phase, int percent, std::string_view message) {
    const auto& spec = progressSpec(phase);
    publish(spec, percent, message, true);
}

void ProgressReporter::setWeighted(ProgressPhase phase,
                                   std::uint64_t current,
                                   std::optional<std::uint64_t> total,
                                   std::string_view message) {
    const auto& spec = progressSpec(phase);
    publish(spec, mapWeighted(spec, current, total), message, false);
}

void ProgressReporter::publish(const ProgressSpec& spec,
                               int percent,
                               std::string_view message,
                               bool force) {
    percent = std::clamp(percent, 0, 100);

    if (!shouldPublish(percent, force)) {
        return;
    }

    const std::string final_message =
        message.empty() ? std::string(spec.message) : std::string(message);

    machine_.setProgress(spec.state,
                         std::string(spec.step),
                         percent,
                         final_message);

    last_percent_ = percent;
    last_publish_ = std::chrono::steady_clock::now();
}

int ProgressReporter::mapWeighted(const ProgressSpec& spec,
                                  std::uint64_t current,
                                  std::optional<std::uint64_t> total) {
    if (!total || *total == 0) {
        return spec.start_percent;
    }

    const auto clamped_current = std::min(current, *total);

    const double ratio =
        static_cast<double>(clamped_current) /
        static_cast<double>(*total);

    const int range = spec.end_percent - spec.start_percent;

    const int percent =
        spec.start_percent +
        static_cast<int>(std::lround(ratio * static_cast<double>(range)));

    return std::clamp(percent, spec.start_percent, spec.end_percent);
}

bool ProgressReporter::shouldPublish(int percent, bool force) const {
    if (force) {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();

    const bool first_publish = last_percent_ < 0;
    const bool percent_changed_enough =
        first_publish ||
        std::abs(percent - last_percent_) >= config_.min_percent_delta;

    const bool interval_elapsed =
        last_publish_.time_since_epoch().count() == 0 ||
        now - last_publish_ >= config_.min_publish_interval;

    return percent_changed_enough && interval_elapsed;
}

ByteProgressTracker::ByteProgressTracker(ProgressReporter& reporter,
                                         ProgressPhase phase,
                                         std::optional<std::uint64_t> total_bytes)
    : reporter_(reporter),
      phase_(phase),
      total_bytes_(total_bytes) {
}

void ByteProgressTracker::begin(std::string_view message) {
    reporter_.begin(phase_, message);
}

void ByteProgressTracker::add(std::size_t bytes, std::string_view message) {
    current_bytes_ += static_cast<std::uint64_t>(bytes);
    reporter_.setWeighted(phase_, current_bytes_, total_bytes_, message);
}

void ByteProgressTracker::set(std::uint64_t current_bytes, std::string_view message) {
    current_bytes_ = current_bytes;
    reporter_.setWeighted(phase_, current_bytes_, total_bytes_, message);
}

void ByteProgressTracker::complete(std::string_view message) {
    reporter_.complete(phase_, message);
}

}  // namespace aegis