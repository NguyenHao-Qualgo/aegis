#include "aegis/slot.h"
#include "aegis/utils.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace aegis {

namespace {

std::string find_boot_slot_from_cmdline() {
    std::ifstream f("/proc/cmdline");
    if (!f) {
        return {};
    }

    std::string cmdline;
    std::getline(f, cmdline);

    for (const char* prefix : {"aegis.slot=", "AEGIS_SLOT="}) {
        auto pos = cmdline.find(prefix);
        if (pos != std::string::npos) {
            pos += std::strlen(prefix);
            auto end = cmdline.find(' ', pos);
            return cmdline.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        }
    }

    return {};
}

std::string find_root_device() {
    std::ifstream f("/proc/mounts");
    if (!f) {
        return {};
    }

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string dev, mount, fstype;
        iss >> dev >> mount >> fstype;
        if (mount == "/") {
            return dev;
        }
    }

    return {};
}

} // namespace

std::string detect_booted_slot(const std::map<std::string, Slot>& slots) {
    const auto from_cmdline = find_boot_slot_from_cmdline();
    if (!from_cmdline.empty()) {
        for (const auto& [name, slot] : slots) {
            if (slot.name == from_cmdline || slot.bootname == from_cmdline) {
                LOG_DEBUG("Booted slot from cmdline: %s -> %s",
                          from_cmdline.c_str(), slot.name.c_str());
                return slot.name;
            }
        }
    }

    const auto root_dev = find_root_device();
    if (!root_dev.empty()) {
        for (const auto& [name, slot] : slots) {
            if (slot.device == root_dev) {
                LOG_DEBUG("Booted slot from root device match: %s (device=%s)",
                          slot.name.c_str(), root_dev.c_str());
                return slot.name;
            }
        }
    }

    LOG_WARNING("Could not detect booted slot");
    return {};
}

std::map<std::string, Slot*> get_target_group(std::map<std::string, Slot>& slots,
                                              const std::string& booted_slot_name) {
    std::map<std::string, Slot*> targets;

    Slot* booted = nullptr;
    for (auto& [name, slot] : slots) {
        if (slot.name == booted_slot_name) {
            booted = &slot;
            break;
        }
    }

    if (!booted) {
        LOG_WARNING("No booted slot found for target selection");
        return targets;
    }

    for (auto& [name, slot] : slots) {
        if (slot.readonly || slot.is_booted) {
            continue;
        }

        if (slot.slot_class == booted->slot_class && slot.index != booted->index) {
            targets[slot.slot_class] = &slot;
        }
    }

    for (auto& [name, slot] : slots) {
        if (slot.readonly || slot.is_booted) {
            continue;
        }

        if (targets.find(slot.slot_class) == targets.end()) {
            targets[slot.slot_class] = &slot;
        }
    }

    return targets;
}

} // namespace aegis