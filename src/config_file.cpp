#include "aegis/config_file.h"
#include "aegis/utils.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace aegis {

Bootloader bootloader_from_string(const std::string& s) {
    if (s == "uboot")
        return Bootloader::UBoot;
    if (s == "custom")
        return Bootloader::Custom;
    if (s == "noop")
        return Bootloader::Noop;
    throw ConfigError("Unknown bootloader type: " + s);
}

const char* to_string(Bootloader b) {
    switch (b) {
    case Bootloader::UBoot:
        return "uboot";
    case Bootloader::Custom:
        return "custom";
    case Bootloader::Noop:
        return "noop";
    }
    return "unknown";
}

SlotType slot_type_from_string(const std::string& s) {
    if (s == "raw")
        return SlotType::Raw;
    if (s == "ext4")
        return SlotType::Ext4;
    if (s == "vfat")
        return SlotType::Vfat;
    if (s == "ubifs")
        return SlotType::Ubifs;
    if (s == "ubivol")
        return SlotType::Ubivol;
    if (s == "nand")
        return SlotType::Nand;
    if (s == "nor")
        return SlotType::Nor;
    if (s == "jffs2")
        return SlotType::Jffs2;
    if (s == "boot-emmc")
        return SlotType::BootEmmc;
    if (s == "boot-mbr-switch")
        return SlotType::BootMbrSwitch;
    if (s == "boot-gpt-switch")
        return SlotType::BootGptSwitch;
    if (s == "boot-raw-fallback")
        return SlotType::BootRawFallback;
    throw ConfigError("Unknown slot type: " + s);
}

const char* to_string(SlotType t) {
    switch (t) {
    case SlotType::Raw:
        return "raw";
    case SlotType::Ext4:
        return "ext4";
    case SlotType::Vfat:
        return "vfat";
    case SlotType::Ubifs:
        return "ubifs";
    case SlotType::Ubivol:
        return "ubivol";
    case SlotType::Nand:
        return "nand";
    case SlotType::Nor:
        return "nor";
    case SlotType::Jffs2:
        return "jffs2";
    case SlotType::BootEmmc:
        return "boot-emmc";
    case SlotType::BootMbrSwitch:
        return "boot-mbr-switch";
    case SlotType::BootGptSwitch:
        return "boot-gpt-switch";
    case SlotType::BootRawFallback:
        return "boot-raw-fallback";
    }
    return "unknown";
}

using IniSection = std::map<std::string, std::string>;
using IniFile = std::map<std::string, IniSection>;

static IniFile parse_ini(const std::string& path) {
    IniFile ini;
    std::ifstream f(path);
    if (!f)
        throw ConfigError("Cannot open config file: " + path);

    std::string line, current_section;
    while (std::getline(f, line)) {
        // Trim
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);
        auto end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            line = line.substr(0, end + 1);

        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            // Trim key/value
            auto kt = key.find_last_not_of(" \t");
            if (kt != std::string::npos)
                key = key.substr(0, kt + 1);
            auto vs = val.find_first_not_of(" \t");
            if (vs != std::string::npos)
                val = val.substr(vs);
            ini[current_section][key] = val;
        }
    }
    return ini;
}

static std::string ini_get(const IniSection& sec, const std::string& key,
                           const std::string& def = {}) {
    auto it = sec.find(key);
    return (it != sec.end()) ? it->second : def;
}

static bool ini_get_bool(const IniSection& sec, const std::string& key, bool def = false) {
    auto it = sec.find(key);
    if (it == sec.end())
        return def;
    return (it->second == "true" || it->second == "yes" || it->second == "1");
}

SystemConfig parse_system_config(const std::string& path) {
    auto ini = parse_ini(path);
    SystemConfig cfg;
    std::string base_dir = dirname(path);

    // [system]
    if (auto it = ini.find("system"); it != ini.end()) {
        auto& s = it->second;
        cfg.compatible = ini_get(s, "compatible");
        cfg.mount_prefix = ini_get(s, "mountprefix", "/mnt/aegis/");
        cfg.statusfile = ini_get(s, "statusfile");
        cfg.data_directory = ini_get(s, "data-directory");
        cfg.system_variant = ini_get(s, "variant-name");
        cfg.variant_dtb = ini_get(s, "variant-dtb");
        cfg.variant_file = ini_get(s, "variant-file");
        cfg.activate_installed = ini_get_bool(s, "activate-installed", true);
        cfg.max_bundle_download_size = ini_get(s, "max-bundle-download-size");

        auto bl = ini_get(s, "bootloader", "uboot");
        cfg.bootloader = bootloader_from_string(bl);

        auto attempts = ini_get(s, "boot-attempts-primary");
        if (!attempts.empty())
            cfg.boot_attempts_primary = std::stoul(attempts);
    }

    // [keyring]
    if (auto it = ini.find("keyring"); it != ini.end()) {
        auto& s = it->second;
        cfg.keyring_path = resolve_path(base_dir, ini_get(s, "path"));
        cfg.keyring_directory = resolve_path(base_dir, ini_get(s, "directory"));
        cfg.keyring_allow_partial_chain = ini_get_bool(s, "allow-partial-chain");
        cfg.keyring_check_crl = ini_get_bool(s, "check-crl");
    }

    // [autoinstall]
    if (auto it = ini.find("autoinstall"); it != ini.end()) {
        cfg.autoinstall_path = ini_get(it->second, "path");
    }

    // [handlers]
    if (auto it = ini.find("handlers"); it != ini.end()) {
        auto& s = it->second;
        cfg.handler_system_info = resolve_path(base_dir, ini_get(s, "system-info"));
        cfg.handler_pre_install = resolve_path(base_dir, ini_get(s, "pre-install"));
        cfg.handler_post_install = resolve_path(base_dir, ini_get(s, "post-install"));
        cfg.handler_bootloader_custom_backend =
            resolve_path(base_dir, ini_get(s, "bootloader-custom-backend"));
    }

    // [encryption]
    if (auto it = ini.find("encryption"); it != ini.end()) {
        auto& s = it->second;
        cfg.encryption_key = resolve_path(base_dir, ini_get(s, "key"));
        cfg.encryption_cert = resolve_path(base_dir, ini_get(s, "cert"));
    }

    // [slot.*] sections
    const std::string slot_prefix = "slot.";
    for (auto& [section_name, sec] : ini) {
        if (section_name.substr(0, slot_prefix.size()) != slot_prefix)
            continue;

        // section_name = "slot.rootfs.0"
        std::string slot_id = section_name.substr(slot_prefix.size());
        auto last_dot = slot_id.rfind('.');
        if (last_dot == std::string::npos)
            throw ConfigError("Invalid slot section: " + section_name);

        Slot slot;
        slot.name = slot_id;
        slot.slot_class = slot_id.substr(0, last_dot);
        slot.index = std::stoi(slot_id.substr(last_dot + 1));
        slot.device = ini_get(sec, "device");
        slot.bootname = ini_get(sec, "bootname");
        slot.readonly = ini_get_bool(sec, "readonly");
        slot.parent_name = ini_get(sec, "parent");

        auto type_str = ini_get(sec, "type", "raw");
        slot.type = slot_type_from_string(type_str);

        cfg.slots[slot_id] = std::move(slot);
    }

    // Resolve parent pointers
    for (auto& [name, slot] : cfg.slots) {
        if (!slot.parent_name.empty()) {
            auto it = cfg.slots.find(slot.parent_name);
            if (it != cfg.slots.end())
                slot.parent = &it->second;
        }
    }

    return cfg;
}

} // namespace aegis
