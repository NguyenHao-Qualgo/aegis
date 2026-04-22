#include "aegis/installer/install_signal_scope.hpp"

#include "aegis/common/error.hpp"

namespace aegis {

namespace {

volatile sig_atomic_t g_install_cancel_signal = 0;

extern "C" void handle_install_signal(int) {
    g_install_cancel_signal = 1;
}

}  // namespace

ScopedInstallSignalHandlers::ScopedInstallSignalHandlers() {
    g_install_cancel_signal = 0;

    struct sigaction ignore_pipe {};
    ignore_pipe.sa_handler = SIG_IGN;
    ::sigemptyset(&ignore_pipe.sa_mask);
    ignore_pipe.sa_flags = 0;
    if (::sigaction(SIGPIPE, &ignore_pipe, &old_pipe_) != 0) {
        fail_runtime("failed to install SIGPIPE handler");
    }

    struct sigaction cancel_action {};
    cancel_action.sa_handler = handle_install_signal;
    ::sigemptyset(&cancel_action.sa_mask);
    cancel_action.sa_flags = 0;
    if (::sigaction(SIGINT, &cancel_action, &old_int_) != 0) {
        fail_runtime("failed to install SIGINT handler");
    }
    if (::sigaction(SIGTERM, &cancel_action, &old_term_) != 0) {
        fail_runtime("failed to install SIGTERM handler");
    }
}

ScopedInstallSignalHandlers::~ScopedInstallSignalHandlers() {
    (void)::sigaction(SIGTERM, &old_term_, nullptr);
    (void)::sigaction(SIGINT, &old_int_, nullptr);
    (void)::sigaction(SIGPIPE, &old_pipe_, nullptr);
    g_install_cancel_signal = 0;
}

const volatile sig_atomic_t* ScopedInstallSignalHandlers::cancel_signal_flag() noexcept {
    return &g_install_cancel_signal;
}

}  // namespace aegis
