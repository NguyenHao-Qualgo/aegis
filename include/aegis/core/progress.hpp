#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "aegis/core/types.hpp"

namespace aegis {

class OtaStateMachine;

enum class ProgressPhase {
    Download,

    InstallVerify,
    InstallPayload,
    InstallFinalize,
    InstallDone,

    RebootReady,

    CommitCheck,
    CommitDone,
};

struct ProgressSpec {
    OtaState state;
    std::string_view step;
    int start_percent;
    int end_percent;
    std::string_view message;
};

class ProgressReporter {
public:
    struct Config {
        std::chrono::milliseconds min_publish_interval{250};
        int min_percent_delta = 3;
    };

    explicit ProgressReporter(OtaStateMachine& machine);
    ProgressReporter(OtaStateMachine& machine, Config config);

    void begin(ProgressPhase phase, std::string_view message = {});
    void complete(ProgressPhase phase, std::string_view message = {});

    void setWeighted(ProgressPhase phase,
                     std::uint64_t current,
                     std::optional<std::uint64_t> total,
                     std::string_view message = {});

private:
    void publish(const ProgressSpec& spec,
                 int percent,
                 std::string_view message,
                 bool force);

    static int mapWeighted(const ProgressSpec& spec,
                           std::uint64_t current,
                           std::optional<std::uint64_t> total);

    bool shouldPublish(int percent, bool force) const;

    OtaStateMachine& machine_;
    Config config_;

    int last_percent_ = -1;
    std::chrono::steady_clock::time_point last_publish_{};
};

class ByteProgressTracker {
public:
    ByteProgressTracker(ProgressReporter& reporter,
                        ProgressPhase phase,
                        std::optional<std::uint64_t> total_bytes);

    void begin(std::string_view message = {});
    void add(std::size_t bytes, std::string_view message = {});
    void complete(std::string_view message = {});

private:
    ProgressReporter& reporter_;
    ProgressPhase phase_;
    std::optional<std::uint64_t> total_bytes_;
    std::uint64_t current_bytes_ = 0;
};

const ProgressSpec& progressSpec(ProgressPhase phase);

}  // namespace aegis