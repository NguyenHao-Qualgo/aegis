#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace aegis {

/// Slot storage types
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

/// Status of a slot (persisted in status file)
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
    std::string status; ///< "ok" or "pending"
};

/// A single slot definition from system.conf
struct Slot {
    std::string name;       ///< e.g. "rootfs.0"
    std::string slot_class; ///< e.g. "rootfs"
    int index = 0;          ///< e.g. 0
    std::string device;     ///< e.g. "/dev/mmcblk0p2"
    SlotType type = SlotType::Raw;
    std::string bootname; ///< bootloader-side name
    bool readonly = false;
    bool install_same = false;
    std::string parent_name; ///< parent slot name (for grouping)
    Slot* parent = nullptr;
    std::string extra_mount_opts;

    SlotStatus status;
    bool is_booted = false;

    /// Unique identifier: class.index
    [[nodiscard]] std::string full_name() const {
        return slot_class + "." + std::to_string(index);
    }
};

/// Determine the currently booted slot from /proc/cmdline + mount info
std::string detect_booted_slot(const std::map<std::string, Slot>& slots);

/// Get the inactive target group for installation
std::map<std::string, Slot*> get_target_group(std::map<std::string, Slot>& slots,
                                              const std::string& booted_slot_name);

} // namespace aegis
