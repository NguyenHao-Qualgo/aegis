#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace aegis {

enum class SlotType {
    Raw,
    Ext4,
    Vfat,
    Ubifs,
    Ubivol,
    Nand,
    Nor,
    Jffs2,
    BootEmmc,
    BootMbrSwitch,
    BootGptSwitch,
    BootRawFallback,
};

SlotType slot_type_from_string(const std::string& s);
const char* to_string(SlotType t);

struct SlotStatus {
    std::string bundle_compatible;
    std::string bundle_version;
    std::string bundle_description;
    std::string bundle_build;
    std::string bundle_hash;

    std::string checksum_sha256;
    uint64_t checksum_size = 0;

    std::string installed_timestamp;
    uint32_t installed_count = 0;

    std::string activated_timestamp;
    uint32_t activated_count = 0;

    // "", "ok", "bad"
    std::string status;
};

struct Slot {
    std::string name;
    std::string slot_class;
    int index = 0;
    std::string device;
    SlotType type = SlotType::Raw;
    std::string bootname;
    bool readonly = false;
    bool install_same = false;

    std::string parent_name;
    Slot* parent = nullptr;
    std::string extra_mount_opts;

    SlotStatus status;
    bool is_booted = false;

    [[nodiscard]] std::string full_name() const {
        return slot_class + "." + std::to_string(index);
    }
};

std::string detect_booted_slot(const std::map<std::string, Slot>& slots);

std::map<std::string, Slot*> get_target_group(std::map<std::string, Slot>& slots,
                                              const std::string& booted_slot_name);

} // namespace aegis