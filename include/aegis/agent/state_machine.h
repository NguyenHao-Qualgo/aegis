#pragma once

#include "aegis/dbus/service_state.h"
#include "aegis/error.h"
#include "aegis/install.h"
#include "aegis/agent/session.h"
#include "aegis/agent/session_store.h"

#include <functional>
#include <string>

namespace aegis {

class OtaStateMachine {
  public:
    using InstallInvoker = std::function<Result<void>(const std::string&, InstallArgs&)>;
    using CompletedEmitter = std::function<void(int)>;
    using PropertyEmitter = std::function<void()>;
    using RebootRequester = std::function<Result<void>()>;

    OtaStateMachine(ServiceState& service_state, OtaSessionStore store, InstallInvoker invoker,
                    CompletedEmitter completed_emitter, PropertyEmitter property_emitter,
                    RebootRequester reboot_requester);

    Result<void> start_update(const std::string& source, const InstallArgs& args);
    Result<void> resume_after_boot();

  private:
    Result<void> handle_download();
    Result<void> handle_install(const InstallArgs& args);
    Result<void> handle_commit();

    Result<void> transition_to(OtaState new_state, int progress, const std::string& message);
    Result<void> fail(const std::string& error);
    Result<void> persist();
    void sync_service_state();
    std::string detect_expected_slot() const;

    ServiceState& service_state_;
    OtaSessionStore store_;
    InstallInvoker install_invoker_;
    CompletedEmitter completed_emitter_;
    PropertyEmitter property_emitter_;
    RebootRequester reboot_requester_;
    OtaSession session_;
};

} // namespace aegis
