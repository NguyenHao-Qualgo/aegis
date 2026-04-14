#include "aegis/slot.h"
#include "aegis/utils.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <cstring>

namespace aegis {

/// Parse /proc/cmdline to find AEGIS_SLOT= or aegis.slot= parameter.
static std::string find_boot_slot_from_cmdline() {
    std::ifstream f("/proc/cmdline");
    if (!f) return {};

    std::string cmdline;
    std::getline(f, cmdline);

    // Look for aegis.slot=<bootname> or AEGIS_SLOT=<bootname>.
    for (auto& prefix : {"aegis.slot=", "AEGIS_SLOT="}) {
        auto pos = cmdline.find(prefix);
        if (pos != std::string::npos) {
            pos += std::strlen(prefix);
            auto end = cmdline.find(' ', pos);
            return cmdline.substr(pos, end == std::string::npos ? end : end - pos);
        }
    }
    return {};
}

/// Try to determine the booted root device from /proc/mounts
static std::string find_root_device() {
    std::ifstream f("/proc/mounts");
    if (!f) return {};

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string dev, mount, fstype;
        iss >> dev >> mount >> fstype;
        if (mount == "/") return dev;
    }
    return {};
}

std::string detect_booted_slot(const std::map<std::string, Slot>& slots) {
    // Priority 1: kernel command line
    auto from_cmdline = find_boot_slot_from_cmdline();
    if (!from_cmdline.empty()) {
        LOG_DEBUG("Booted slot from cmdline: %s", from_cmdline.c_str());
        return from_cmdline;
    }

    // Priority 2: match root device to slot device
    auto root_dev = find_root_device();
    if (!root_dev.empty()) {
        for (auto& [name, slot] : slots) {
            if (slot.device == root_dev) {
                LOG_DEBUG("Booted slot from root device match: %s (device=%s)",
                         slot.bootname.c_str(), root_dev.c_str());
                return slot.bootname.empty() ? slot.name : slot.bootname;
            }
        }
    }

    LOG_WARNING("Could not detect booted slot");
    return {};
}

std::map<std::string, Slot*> get_target_group(
    std::map<std::string, Slot>& slots,
    const std::string& booted_slot_name) {
    // Find the booted slot
    Slot* booted = nullptr;
    for (auto& [name, slot] : slots) {
        if (slot.bootname == booted_slot_name || slot.name == booted_slot_name) {
            booted = &slot;
            break;
        }
    }

    // Determine the booted slot's class group
    std::set<std::string> active_group_indices;
    if (booted) {
        // Find all slots with the same class and index as the booted slot
        // In a 2-slot A/B setup, if rootfs.0 is booted, rootfs.1 is the target
        active_group_indices.insert(std::to_string(booted->index));
    }

    // Collect inactive slots grouped by class
    std::map<std::string, Slot*> targets;
    for (auto& [name, slot] : slots) {
        if (slot.readonly) continue;
        if (slot.is_booted) continue;

        // Skip slots in the same group as the booted slot
        bool is_active = false;
        if (booted && slot.slot_class == booted->slot_class &&
            slot.index == booted->index) {
            is_active = true;
        }
        // Also skip child slots whose parent is in the active group
        if (slot.parent && slot.parent->is_booted) {
            is_active = true;
        }

        if (!is_active) {
            targets[slot.slot_class] = &slot;
        }
    }

    return targets;
}

} // namespace aegis
