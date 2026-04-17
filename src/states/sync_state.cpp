#include "aegis/states/sync_state.hpp"

#include <memory>

#include "aegis/ota_context.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/verify_bundle_state.hpp"

namespace aegis {

void SyncState::onEnter(OtaContext& ctx) {
    try {
        ctx.status_.state = OtaState::Sync;
        ctx.status_.operation = "sync";
        ctx.status_.progress = 0;
        ctx.status_.message = "Collecting current slot state";
        ctx.status_.bootedSlot = ctx.bootControl_.getBootedSlot();
        ctx.status_.primarySlot = ctx.bootControl_.getPrimarySlot();
        ctx.save();

        ctx.transitionTo(std::make_unique<VerifyBundleState>());
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

void SyncState::handle(OtaContext&, const OtaEvent&) {
}

}  // namespace aegis