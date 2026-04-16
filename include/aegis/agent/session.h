#pragma once

#include <string>

namespace aegis {

enum class OtaState {
    IdleSync,
    Download,
    Install,
    Reboot,
    Commit,
    Failure,
};

const char* to_string(OtaState state);
OtaState ota_state_from_string(const std::string& value);

struct OtaSession {
    std::string transaction_id;
    OtaState state = OtaState::IdleSync;

    std::string source;
    std::string bundle_path;
    std::string bundle_version;
    std::string expected_slot;
    std::string booted_slot;

    std::string status_message = "idle";
    std::string last_error;
    int progress = 0;

    bool fc_state_allowed = true;
    bool download_verified = false;
    bool install_done = false;
    bool reboot_requested = false;
    bool commit_done = false;
};

} // namespace aegis
