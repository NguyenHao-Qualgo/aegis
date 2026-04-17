#include "aegis/status_file.h"
#include "aegis/utils.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace aegis {

std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

// Simple INI serializer for slot status
static std::string serialize_slot_status(const std::string& slot_name, const SlotStatus& s) {
    std::ostringstream out;
    out << "[slot." << slot_name << "]\n";
    if (!s.bundle_compatible.empty())
        out << "bundle.compatible=" << s.bundle_compatible << "\n";
    if (!s.bundle_version.empty())
        out << "bundle.version=" << s.bundle_version << "\n";
    if (!s.bundle_description.empty())
        out << "bundle.description=" << s.bundle_description << "\n";
    if (!s.bundle_build.empty())
        out << "bundle.build=" << s.bundle_build << "\n";
    if (!s.bundle_hash.empty())
        out << "bundle.hash=" << s.bundle_hash << "\n";
    if (!s.checksum_sha256.empty())
        out << "sha256=" << s.checksum_sha256 << "\n";
    if (s.checksum_size > 0)
        out << "size=" << s.checksum_size << "\n";
    if (!s.installed_timestamp.empty())
        out << "installed.timestamp=" << s.installed_timestamp << "\n";
    out << "installed.count=" << s.installed_count << "\n";
    if (!s.activated_timestamp.empty())
        out << "activated.timestamp=" << s.activated_timestamp << "\n";
    out << "activated.count=" << s.activated_count << "\n";
    if (!s.status.empty())
        out << "status=" << s.status << "\n";
    out << "\n";
    return out.str();
}

// Parse a status INI section into SlotStatus
static void parse_status_section(const std::map<std::string, std::string>& sec, SlotStatus& s) {
    auto get = [&](const std::string& k) -> std::string {
        auto it = sec.find(k);
        return it != sec.end() ? it->second : "";
    };
    s.bundle_compatible = get("bundle.compatible");
    s.bundle_version = get("bundle.version");
    s.bundle_description = get("bundle.description");
    s.bundle_build = get("bundle.build");
    s.bundle_hash = get("bundle.hash");
    s.checksum_sha256 = get("sha256");
    auto sz = get("size");
    if (!sz.empty())
        s.checksum_size = std::stoull(sz);
    s.installed_timestamp = get("installed.timestamp");
    auto ic = get("installed.count");
    if (!ic.empty())
        s.installed_count = std::stoul(ic);
    s.activated_timestamp = get("activated.timestamp");
    auto ac = get("activated.count");
    if (!ac.empty())
        s.activated_count = std::stoul(ac);
    s.status = get("status");
}

Result<void> load_slot_status(Slot& slot, const std::string& data_directory) {
    std::string path = data_directory + "/" + slot.name + ".status";
    if (!path_exists(path))
        return Result<void>::ok(); // No status yet

    std::ifstream f(path);
    if (!f)
        return Result<void>::err("Cannot open status file: " + path);

    std::map<std::string, std::string> kvs;
    std::string line;
    while (std::getline(f, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);
        if (line.empty() || line[0] == '#' || line[0] == '[')
            continue;

        auto eq = line.find('=');
        if (eq != std::string::npos) {
            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);
            auto kt = key.find_last_not_of(" \t");
            if (kt != std::string::npos)
                key = key.substr(0, kt + 1);
            auto vs = val.find_first_not_of(" \t");
            if (vs != std::string::npos)
                val = val.substr(vs);
            kvs[key] = val;
        }
    }

    parse_status_section(kvs, slot.status);
    return Result<void>::ok();
}

Result<void> save_slot_status(const Slot& slot, const std::string& data_directory) {
    mkdir_p(data_directory);
    std::string path = data_directory + "/" + slot.name + ".status";
    std::string content = serialize_slot_status(slot.name, slot.status);
    write_text_file(path, content);
    return Result<void>::ok();
}

Result<void> load_all_slot_status(std::map<std::string, Slot>& slots,
                                  const std::string& status_file_path) {
    if (!path_exists(status_file_path))
        return Result<void>::ok();

    std::ifstream f(status_file_path);
    if (!f)
        return Result<void>::err("Cannot open: " + status_file_path);

    // Parse all sections
    using Section = std::map<std::string, std::string>;
    std::map<std::string, Section> sections;
    std::string current_section, line;

    while (std::getline(f, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);
        auto end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            line = line.substr(0, end + 1);
        if (line.empty() || line[0] == '#')
            continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);
            auto kt = key.find_last_not_of(" \t");
            if (kt != std::string::npos)
                key = key.substr(0, kt + 1);
            auto vs = val.find_first_not_of(" \t");
            if (vs != std::string::npos)
                val = val.substr(vs);
            sections[current_section][key] = val;
        }
    }

    const std::string prefix = "slot.";
    for (auto& [sec_name, sec] : sections) {
        if (sec_name.substr(0, prefix.size()) != prefix)
            continue;
        std::string slot_name = sec_name.substr(prefix.size());
        auto it = slots.find(slot_name);
        if (it != slots.end()) {
            parse_status_section(sec, it->second.status);
        }
    }

    return Result<void>::ok();
}

Result<void> save_all_slot_status(const std::map<std::string, Slot>& slots,
                                  const std::string& status_file_path) {
    std::ofstream f(status_file_path);
    if (!f)
        return Result<void>::err("Cannot write: " + status_file_path);

    for (auto& [name, slot] : slots) {
        f << serialize_slot_status(name, slot.status);
    }
    return Result<void>::ok();
}

Result<void> FileStatusStore::save_slot(const Slot& slot) {
    if (!config_.data_directory.empty()) {
        auto result = save_slot_status(slot, config_.data_directory);
        if (!result) {
            return result;
        }
    }

    if (!config_.statusfile.empty()) {
        return save_all(config_.slots);
    }

    return Result<void>::ok();
}

Result<void> FileStatusStore::save_all(const std::map<std::string, Slot>& slots) {
    if (!config_.statusfile.empty()) {
        return save_all_slot_status(slots, config_.statusfile);
    }
    return Result<void>::ok();
}

} // namespace aegis
