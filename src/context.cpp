#include "aegis/context.h"
#include "aegis/bootchooser.h"
#include "aegis/slot.h"
#include "aegis/status_file.h"
#include "aegis/utils.h"

namespace aegis {

Context& Context::instance() {
    static Context ctx;
    return ctx;
}

void Context::init(const std::string& config_path, const std::string& cert_path,
                   const std::string& key_path, const std::string& keyring_path,
                   const std::string& override_boot_slot, const std::string& mount_prefix) {
    config_path_ = config_path;
    cert_path_ = cert_path;
    key_path_ = key_path;

    // Parse system configuration
    if (!config_path_.empty() && path_exists(config_path_)) {
        config_ = parse_system_config(config_path_);
    }

    // Override keyring from command line
    keyring_path_ = keyring_path.empty() ? config_.keyring_path : keyring_path;

    // Override mount prefix
    mount_prefix_ = mount_prefix.empty() ? config_.mount_prefix : mount_prefix;

    // Detect booted slot
    if (!override_boot_slot.empty()) {
        boot_slot_ = override_boot_slot;
    } else {
        boot_slot_ = detect_booted_slot(config_.slots);
    }

    // Mark the booted slot
    for (auto& [name, slot] : config_.slots) {
        slot.is_booted = (slot.bootname == boot_slot_ || slot.name == boot_slot_);
    }

    // Load slot status from data directory or status file
    if (!config_.statusfile.empty() && path_exists(config_.statusfile)) {
        load_all_slot_status(config_.slots, config_.statusfile);
    } else if (!config_.data_directory.empty()) {
        for (auto& [name, slot] : config_.slots) {
            load_slot_status(slot, config_.data_directory);
        }
    }

    bootchooser_ = create_bootchooser(config_);

    initialized_ = true;
    LOG_INFO("Context initialized: compatible=%s, bootloader=%s, boot_slot=%s",
             config_.compatible.c_str(), to_string(config_.bootloader), boot_slot_.c_str());
}

} // namespace aegis
