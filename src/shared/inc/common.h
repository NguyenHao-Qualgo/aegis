#pragma once

#include <chrono>
#include <string>

#include "HttpStatusCode.h"
using HttpStatus::HttpStatusCode;

namespace steady {
using Time = std::chrono::steady_clock::time_point;

static Time Now() {
    return std::chrono::steady_clock::now();
};

static long long MillisFromNow(Time& t) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Now() - t).count();
};

static long double SecondsFromNow(Time& t) {
    return static_cast<double>(MillisFromNow(t)) / 1000;
};
}  // namespace steady

using CommandResponse = struct CommandResponse {
    HttpStatusCode code{};
    std::string response{};
};

enum STREAM_STARTSTOP_RESPONSE {
    STREAM_STARTSTOP_SUCCESS,
    STREAM_STARTSTOP_FAIL_INTERNAL,
    STREAM_STARTSTOP_FAIL_TOO_MANY_REQUESTS,
    STREAM_STARTSTOP_STREAM_NOT_FOUND,
    STREAM_STARTSTOP_FAIL_UNKNOWN,
    STREAM_STARTSTOP_NOT_ACTIVE
};