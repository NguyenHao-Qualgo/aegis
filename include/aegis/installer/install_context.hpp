#pragma once

#include <csignal>
#include <stop_token>

#include "aegis/common/error.hpp"

namespace aegis {

class OtaStateMachine;

struct InstallContext {
    OtaStateMachine& machine;
    std::stop_token stop;
    const volatile sig_atomic_t* signal_stop = nullptr;

    void check_cancel() const {
        if ((signal_stop != nullptr && *signal_stop != 0) || stop.stop_requested()) {
            fail_runtime("Installation cancelled");
        }
    }
};

}  // namespace aegis
