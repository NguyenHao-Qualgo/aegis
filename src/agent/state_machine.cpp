#include "aegis/agent/state_machine.h"

#include "aegis/bootchooser.h"
#include "aegis/context.h"
#include "aegis/mark.h"
#include "aegis/utils.h"

#include <cstdlib>

namespace aegis {

OtaStateMachine::OtaStateMachine(ServiceState& service_state, OtaSessionStore store,
                                 InstallInvoker invoker, CompletedEmitter completed_emitter,
                                 PropertyEmitter property_emitter,
                                 RebootRequester reboot_requester)
    : service_state_(service_state), store_(std::move(store)),
      install_invoker_(std::move(invoker)), completed_emitter_(std::move(completed_emitter)),
      property_emitter_(std::move(property_emitter)),
      reboot_requester_(std::move(reboot_requester)) {
    auto loaded = store_.load();
    if (loaded) {
        session_ = std::move(loaded.value());
        sync_service_state();
    }
}

Result<void> OtaStateMachine::start_update(const std::string& source, const InstallArgs& args) {
    session_ = {};
    session_.transaction_id = args.transaction_id.empty() ? random_hex(8) : args.transaction_id;
    session_.source = source;
    session_.bundle_path = source;
    session_.fc_state_allowed = true;

    auto dl = handle_download();
    if (!dl) {
        completed_emitter_(1);
        return dl;
    }

    auto inst = handle_install(args);
    if (!inst) {
        completed_emitter_(1);
        return inst;
    }

    completed_emitter_(0);
    return Result<void>::ok();
}

Result<void> OtaStateMachine::resume_after_boot() {
    auto loaded = store_.load();
    if (!loaded) {
        return Result<void>::err(loaded.error());
    }
    session_ = std::move(loaded.value());
    sync_service_state();

    if (session_.state != OtaState::Reboot) {
        return Result<void>::ok();
    }

    return handle_commit();
}

Result<void> OtaStateMachine::handle_download() {
    auto t = transition_to(OtaState::Download, 5, "Downloading bundle");
    if (!t) return t;

    if (!session_.fc_state_allowed) {
        return fail("FC state is not allowed for update");
    }

    if (session_.source.empty()) {
        return fail("Bundle source must not be empty");
    }

    if (session_.source.find("://") != std::string::npos) {
        return fail("Remote OTA download is not implemented yet in this build");
    }

    if (!path_exists(session_.source)) {
        return fail("Bundle path not found: " + session_.source);
    }

    session_.download_verified = true;
    session_.bundle_path = session_.source;
    return transition_to(OtaState::Install, 20, "Bundle downloaded and verified");
}

Result<void> OtaStateMachine::handle_install(const InstallArgs& args) {
    auto t = transition_to(OtaState::Install, 25, "Installing bundle to inactive slot");
    if (!t) return t;

    if (!session_.fc_state_allowed) {
        return fail("FC state is not allowed for install");
    }

    InstallArgs install_args = args;
    install_args.transaction_id = session_.transaction_id;
    install_args.status_notify = [this](const std::string& message) {
        service_state_.update_progress(service_state_.progress().percentage, message, 0);
        sync_service_state();
        property_emitter_();
    };
    install_args.progress = [this](int percentage, const std::string& message) {
        session_.progress = percentage;
        service_state_.update_progress(percentage, message, 0);
        sync_service_state();
        persist();
        property_emitter_();
    };

    auto res = install_invoker_(session_.bundle_path, install_args);
    if (!res) {
        return fail(res.error());
    }

    session_.install_done = true;
    session_.expected_slot = detect_expected_slot();
    auto reboot_state = transition_to(OtaState::Reboot, 100, "Install completed, reboot required");
    if (!reboot_state) return reboot_state;

    auto reboot_result = reboot_requester_();
    if (!reboot_result) {
        return fail(reboot_result.error());
    }

    return Result<void>::ok();
}

Result<void> OtaStateMachine::handle_commit() {
    auto t = transition_to(OtaState::Commit, 10, "Committing update after reboot");
    if (!t) return t;

    auto& ctx = Context::instance();
    auto bootchooser = create_bootchooser(ctx.config());
    auto* primary = bootchooser->get_primary(ctx.config().slots);
    session_.booted_slot = primary ? primary->name : ctx.boot_slot();

    if (session_.expected_slot.empty()) {
        return fail("Expected slot missing for commit");
    }
    if (session_.booted_slot != session_.expected_slot) {
        return fail("Booted slot '" + session_.booted_slot +
                    "' does not match expected slot '" + session_.expected_slot + "'");
    }

    auto active = mark_good(session_.expected_slot);
    if (!active) {
        return fail(active.error());
    }

    session_.commit_done = true;
    auto done = transition_to(OtaState::IdleSync, 100, "Update committed successfully");
    if (!done) return done;
    return store_.clear();
}

Result<void> OtaStateMachine::transition_to(OtaState new_state, int progress,
                                            const std::string& message) {
    session_.state = new_state;
    session_.progress = progress;
    session_.status_message = message;
    if (new_state != OtaState::Failure) {
        session_.last_error.clear();
    }
    sync_service_state();
    auto save = persist();
    if (!save) {
        return save;
    }
    property_emitter_();
    return Result<void>::ok();
}

Result<void> OtaStateMachine::fail(const std::string& error) {
    session_.state = OtaState::Failure;
    session_.last_error = error;
    session_.status_message = error;
    sync_service_state();
    persist();
    property_emitter_();
    return Result<void>::err(error);
}

Result<void> OtaStateMachine::persist() { return store_.save(session_); }

void OtaStateMachine::sync_service_state() {
    service_state_.update_ota(session_.state == OtaState::Failure ? "failure" : to_string(session_.state),
                              session_.status_message, session_.transaction_id,
                              session_.expected_slot);
    if (session_.state == OtaState::Failure && !session_.last_error.empty()) {
        service_state_.set_last_error(session_.last_error);
    }
}

std::string OtaStateMachine::detect_expected_slot() const {
    auto& ctx = Context::instance();
    auto bootchooser = create_bootchooser(ctx.config());
    auto* primary = bootchooser->get_primary(ctx.config().slots);
    return primary ? primary->name : std::string{};
}

} // namespace aegis
