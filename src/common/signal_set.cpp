#include "aegis/common/signal_set.hpp"

#include <cerrno>
#include <cstring>
#include <pthread.h>
#include <stdexcept>

namespace aegis {

namespace {

std::runtime_error make_signal_error(const char* action, int error_code) {
    return std::runtime_error(std::string(action) + ": " + std::strerror(error_code));
}

timespec to_timespec(const std::chrono::milliseconds timeout) {
    using namespace std::chrono;

    const auto secs = duration_cast<seconds>(timeout);
    const auto nanos = duration_cast<nanoseconds>(timeout - secs);
    return timespec{
        .tv_sec = secs.count(),
        .tv_nsec = nanos.count(),
    };
}

}  // namespace

SignalSet::SignalSet(const std::initializer_list<int> signals) {
    ::sigemptyset(&set_);
    for (const int signal : signals) {
        ::sigaddset(&set_, signal);
    }
}

void SignalSet::block() const {
    const int rc = ::pthread_sigmask(SIG_BLOCK, &set_, nullptr);
    if (rc != 0) {
        throw make_signal_error("failed to block signals", rc);
    }
}

int SignalSet::wait() const {
    int signum = 0;
    const int rc = ::sigwait(&set_, &signum);
    if (rc != 0) {
        throw make_signal_error("failed waiting for signal", rc);
    }
    return signum;
}

std::optional<int> SignalSet::wait_for(const std::chrono::milliseconds timeout) const {
    const timespec ts = to_timespec(timeout);
    siginfo_t info {};

    const int signum = ::sigtimedwait(&set_, &info, &ts);
    if (signum >= 0) {
        return signum;
    }
    if (errno == EAGAIN) {
        return std::nullopt;
    }
    throw make_signal_error("failed polling for signal", errno);
}

}  // namespace aegis
