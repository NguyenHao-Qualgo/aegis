#include "aegis/config/state_store.hpp"

#include <filesystem>
#include <sstream>

#include "aegis/common/util.hpp"

namespace aegis {

StateStore::StateStore(std::string path) : path_(std::move(path)) {}

OtaStatus StateStore::load() const {
    OtaStatus status;
    if (!fileExists(path_)) {
        return status;
    }
    const auto content = readFile(path_);
    std::istringstream is(content);
    std::string line;
    while (std::getline(is, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const auto key = line.substr(0, pos);
        const auto value = line.substr(pos + 1);
        if (key == "state") {
            if (value == "Idle" || value == "Sync") status.state = OtaState::Idle;
            else if (value == "Download") status.state = OtaState::Download;
            else if (value == "Install") status.state = OtaState::Install;
            else if (value == "Reboot") status.state = OtaState::Reboot;
            else if (value == "Commit") status.state = OtaState::Commit;
            else if (value == "Failure") status.state = OtaState::Failure;
        } else if (key == "operation") status.operation = value;
        else if (key == "progress") status.progress = std::stoi(value);
        else if (key == "message") status.message = value;
        else if (key == "last_error") status.lastError = value;
        else if (key == "booted_slot") status.bootedSlot = value;
        else if (key == "primary_slot") status.primarySlot = value;
        else if (key == "bundle_version") status.bundleVersion = value;
    }
    return status;
}

void StateStore::save(const OtaStatus& status) const {
    std::ostringstream os;
    os << "state=" << toString(status.state) << '\n';
    os << "operation=" << status.operation << '\n';
    os << "progress=" << status.progress << '\n';
    os << "message=" << status.message << '\n';
    os << "last_error=" << status.lastError << '\n';
    os << "booted_slot=" << status.bootedSlot << '\n';
    os << "primary_slot=" << status.primarySlot << '\n';
    os << "bundle_version=" << status.bundleVersion << '\n';
    writeFile(path_, os.str());
}

}  // namespace aegis
