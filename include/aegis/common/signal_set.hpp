#pragma once

#include <chrono>
#include <csignal>
#include <initializer_list>
#include <optional>

namespace aegis {

class SignalSet {
public:
    SignalSet(std::initializer_list<int> signals);

    void block() const;
    int wait() const;
    std::optional<int> wait_for(std::chrono::milliseconds timeout) const;

private:
    sigset_t set_ {};
};

}  // namespace aegis
