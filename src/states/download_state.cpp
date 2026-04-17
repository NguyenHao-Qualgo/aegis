#include "aegis/states/download_state.hpp"

#include <filesystem>
#include <memory>

#include "aegis/ota_context.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/install_state.hpp"

namespace aegis {

static bool isUrl(const std::string& path) {
    return path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0;
}

void DownloadState::onEnter(OtaContext& ctx) {
    try {
        ctx.status_.state = OtaState::Download;
        ctx.status_.operation = "download";
        ctx.status_.progress = 10;

        if (isUrl(ctx.status_.bundlePath)) {
            ctx.status_.message = "Downloading bundle";
            ctx.save();
            ctx.status_.bundlePath = ctx.downloadBundle(ctx.status_.bundlePath);
        } else {
            ctx.status_.message = "Preparing bundle";
            ctx.save();
            if (!std::filesystem::exists(ctx.status_.bundlePath)) {
                throw std::runtime_error("Bundle not found: " + ctx.status_.bundlePath);
            }
        }

        ctx.transitionTo(std::make_unique<InstallState>());
    } catch (const std::exception& e) {
        ctx.transitionTo(std::make_unique<FailureState>(e.what()));
    }
}

void DownloadState::handle(OtaContext&, const OtaEvent&) {
}

}  // namespace aegis
