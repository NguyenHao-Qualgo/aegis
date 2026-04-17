#include "aegis/agent/session_store.h"

#include "aegis/utils.h"

#include <map>
#include <sstream>

namespace aegis {
namespace {

std::string join_path(const std::string& base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    if (base.back() == '/') {
        return base + leaf;
    }
    return base + "/" + leaf;
}

std::string trim(std::string value) {
    auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

bool parse_bool(const std::string& value) {
    return value == "1" || value == "true" || value == "yes";
}

std::string encode_bool(bool value) {
    return value ? "true" : "false";
}

} // namespace

const char* to_string(OtaState state) {
    switch (state) {
    case OtaState::IdleSync:
        return "idle-sync";
    case OtaState::Download:
        return "download";
    case OtaState::Install:
        return "install";
    case OtaState::Reboot:
        return "reboot";
    case OtaState::Commit:
        return "commit";
    case OtaState::Failure:
        return "failure";
    }
    return "idle-sync";
}

OtaState ota_state_from_string(const std::string& value) {
    if (value == "idle-sync") return OtaState::IdleSync;
    if (value == "download") return OtaState::Download;
    if (value == "install") return OtaState::Install;
    if (value == "reboot") return OtaState::Reboot;
    if (value == "commit") return OtaState::Commit;
    if (value == "failure") return OtaState::Failure;
    return OtaState::IdleSync;
}

OtaSessionStore::OtaSessionStore(const SystemConfig& config)
    : path_(default_path(config)) {}

std::string OtaSessionStore::default_path(const SystemConfig& config) {
    if (!config.data_directory.empty()) {
        return join_path(config.data_directory, "aegis/ota-session.json");
    }
    return "/var/lib/aegis/ota-session.json";
}

Result<OtaSession> OtaSessionStore::load() const {
    OtaSession session;
    if (!path_exists(path_)) {
        return Result<OtaSession>::ok(std::move(session));
    }

    std::istringstream in(read_text_file(path_));
    std::string line;
    std::map<std::string, std::string> values;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        values[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }

    session.transaction_id = values["transaction_id"];
    session.state = ota_state_from_string(values["state"]);
    session.source = values["source"];
    session.bundle_path = values["bundle_path"];
    session.bundle_version = values["bundle_version"];
    session.expected_slot = values["expected_slot"];
    session.booted_slot = values["booted_slot"];
    session.status_message = values["status_message"];
    session.last_error = values["last_error"];
    if (!values["progress"].empty()) {
        session.progress = std::stoi(values["progress"]);
    }
    session.fc_state_allowed = parse_bool(values["fc_state_allowed"]);
    session.download_verified = parse_bool(values["download_verified"]);
    session.install_done = parse_bool(values["install_done"]);
    session.reboot_requested = parse_bool(values["reboot_requested"]);
    session.commit_done = parse_bool(values["commit_done"]);

    return Result<OtaSession>::ok(std::move(session));
}

Result<void> OtaSessionStore::save(const OtaSession& session) const {
    auto parent = dirname(path_);
    if (!parent.empty()) {
        auto mk = mkdir_p(parent);
        if (!mk) {
            return mk;
        }
    }

    std::ostringstream out;
    out << "transaction_id=" << session.transaction_id << "\n";
    out << "state=" << to_string(session.state) << "\n";
    out << "source=" << session.source << "\n";
    out << "bundle_path=" << session.bundle_path << "\n";
    out << "bundle_version=" << session.bundle_version << "\n";
    out << "expected_slot=" << session.expected_slot << "\n";
    out << "booted_slot=" << session.booted_slot << "\n";
    out << "status_message=" << session.status_message << "\n";
    out << "last_error=" << session.last_error << "\n";
    out << "progress=" << session.progress << "\n";
    out << "fc_state_allowed=" << encode_bool(session.fc_state_allowed) << "\n";
    out << "download_verified=" << encode_bool(session.download_verified) << "\n";
    out << "install_done=" << encode_bool(session.install_done) << "\n";
    out << "reboot_requested=" << encode_bool(session.reboot_requested) << "\n";
    out << "commit_done=" << encode_bool(session.commit_done) << "\n";

    write_text_file(path_, out.str());
    return Result<void>::ok();
}

Result<void> OtaSessionStore::clear() const {
    if (!path_exists(path_)) {
        return Result<void>::ok();
    }
    auto parent = dirname(path_);
    auto base = basename(path_);
    auto res = run_command({"rm", "-f", path_});
    if (res.first != 0) {
        return Result<void>::err("Failed to remove OTA session file: " + res.second);
    }
    return Result<void>::ok();
}

const std::string& OtaSessionStore::path() const { return path_; }

} // namespace aegis
