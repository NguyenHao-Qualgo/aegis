#pragma once

#include <memory>

namespace aegis {

class OtaContext;
struct OtaEvent;

class IOtaState {
public:
    virtual ~IOtaState() = default;

    virtual const char* name() const = 0;

    virtual void onEnter(OtaContext& ctx) {}
    virtual void onExit(OtaContext& ctx) {}

    virtual void handle(OtaContext& ctx, const OtaEvent& event) = 0;
};

}  // namespace aegis