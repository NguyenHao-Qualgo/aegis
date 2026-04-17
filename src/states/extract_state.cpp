#include "aegis/states/extract_state.hpp"

#include <memory>

#include "aegis/ota_context.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/install_state.hpp"

namespace aegis {

void ExtractState::onEnter(OtaContext& ctx) {
    try {
        ctx.status_.state = OtaState::Install;
        ctx.status_.operation = "extract";
        ctx.status_.progress = 25;
        ctx.status_.message = "Extracting bundle";
        ctx.save();

        ctx.status_.installPath = ctx.extractBundle(ctx.status_.bundlePath);
        ctx.transitionTo(std::make_unique<InstallState>());
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

void ExtractState::handle(OtaContext&, const OtaEvent&) {
}

}  // namespace aegis