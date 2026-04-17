#pragma once

#include "aegis/manifest.h"
#include "aegis/slot.h"

#include <map>
#include <string>
#include <vector>

namespace aegis {

/// Bootloader backends (stripped to u-boot + custom only)
enum class Bootloader {
    UBoot,
    Custom,
    Noop, ///< for testing
};

Bootloader bootloader_from_string(const std::string& s);
const char* to_string(Bootloader b);

/// Parsed system configuration (/etc/aegis/system.conf)
struct SystemConfig {
    // [system] section
    std::string compatible;
    Bootloader bootloader = Bootloader::UBoot;
    uint32_t boot_attempts_primary = 3;
    std::string variant_dtb;
    std::string variant_file;
    std::string variant_name;
    std::string mount_prefix = "/mnt/aegis/";
    bool activate_installed = true;
    std::string statusfile;
    std::string data_directory;
    std::string system_variant;
    std::string max_bundle_download_size;

    // [keyring] section
    std::string keyring_path;
    std::string keyring_directory;
    bool keyring_allow_partial_chain = false;
    bool keyring_check_crl = false;

    // [autoinstall] section
    std::string autoinstall_path;

    // [handlers] section
    std::string handler_system_info;
    std::string handler_pre_install;
    std::string handler_post_install;
    std::string handler_bootloader_custom_backend;

    // [encryption] section
    std::string encryption_key;
    std::string encryption_cert;

    // Slots: key = "class.index"
    std::map<std::string, Slot> slots;
};

/// Parse system.conf from file
SystemConfig parse_system_config(const std::string& path);

} // namespace aegis
