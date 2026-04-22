#pragma once

#include <csignal>

namespace aegis {

class ScopedInstallSignalHandlers {
public:
    ScopedInstallSignalHandlers();
    ~ScopedInstallSignalHandlers();

    ScopedInstallSignalHandlers(const ScopedInstallSignalHandlers&) = delete;
    ScopedInstallSignalHandlers& operator=(const ScopedInstallSignalHandlers&) = delete;

    static const volatile sig_atomic_t* cancel_signal_flag() noexcept;

private:
    struct sigaction old_pipe_ {};
    struct sigaction old_int_ {};
    struct sigaction old_term_ {};
};

}  // namespace aegis
