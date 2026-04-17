#include "aegis/states/verify_bundle_state.hpp"

#include <memory>
#include <stdexcept>

#include "aegis/ota_context.hpp"
#include "aegis/states/extract_state.hpp"
#include "aegis/states/failure_state.hpp"

namespace aegis {

void VerifyBundleState::onEnter(OtaContext& ctx) {
    try {
        ctx.status_.state = OtaState::Download;
        ctx.status_.operation = "verify";
        ctx.status_.progress = 10;
        ctx.status_.message = "Verifying bundle signature";
        ctx.save();

        auto manifest = ctx.verifier_.verifyBundle(ctx.status_.bundlePath, ctx.config_);
        if (manifest) {
            ctx.status_.bundleVersion = manifest->version;
        }

        ctx.transitionTo(std::make_unique<ExtractState>());
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

void VerifyBundleState::handle(OtaContext&, const OtaEvent&) {
}

}  // namespace aegis