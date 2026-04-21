#pragma once

#include <memory>

namespace aegis {

class OtaStateMachine;
struct OtaEvent;

class IOtaState {
public:
    virtual ~IOtaState() = default;

    virtual const char* name() const = 0;

    virtual void onEnter(OtaStateMachine& machine) {}
    virtual void onExit(OtaStateMachine& machine) {}

    virtual void handle(OtaStateMachine& machine, const OtaEvent& event) = 0;
};

}  // namespace aegis
