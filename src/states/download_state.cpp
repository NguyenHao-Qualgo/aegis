#include "aegis/states/download_state.hpp"

#include <filesystem>
#include <memory>

#include "aegis/core/ota_state_machine.hpp"
namespace aegis {

static bool isUrl(const std::string& path) {
    return path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0;
}

void DownloadState::onEnter(OtaStateMachine& machine) {
    try {
        const auto bundlePath = machine.getStatus().bundlePath;
        if (isUrl(bundlePath)) {
            machine.setProgress(OtaState::Download, "download", 10, "Downloading bundle");
            machine.setBundlePath(machine.downloadBundle(bundlePath));
        } else {
            machine.setProgress(OtaState::Download, "download", 10, "Preparing bundle");
            if (!std::filesystem::exists(bundlePath)) {
                throw std::runtime_error("Bundle not found: " + bundlePath);
            }
        }
        machine.transitionToInstall();
    } catch (const std::exception& e) {
        machine.transitionToFailure(e.what());
    }
}

void DownloadState::handle(OtaStateMachine&, const OtaEvent&) {
}

}  // namespace aegis
