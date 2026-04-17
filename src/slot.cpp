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

bool Slot::matches_identifier(std::string_view identifier) const {
    return !identifier.empty() && (name == identifier || bootname == identifier || full_name() == identifier);
}

bool Slot::is_same_class(const Slot& other) const {
    return slot_class == other.slot_class;
}

bool Slot::is_alternate_of(const Slot& other) const {
    return is_same_class(other) && index != other.index;
}

std::string detect_booted_slot(const std::map<std::string, Slot>& slots) {
    const auto from_cmdline = find_boot_slot_from_cmdline();
    if (!from_cmdline.empty()) {
        if (const auto* slot = find_slot_by_identifier(slots, from_cmdline)) {
            LOG_DEBUG("Booted slot from cmdline: %s -> %s",
                      from_cmdline.c_str(), slot->name.c_str());
            return slot->name;
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

Slot* find_booted_slot(std::map<std::string, Slot>& slots) {
    for (auto& [name, slot] : slots) {
        if (slot.is_booted) {
            return &slot;
        }
    }
    return nullptr;
}

const Slot* find_booted_slot(const std::map<std::string, Slot>& slots) {
    for (const auto& [name, slot] : slots) {
        if (slot.is_booted) {
            return &slot;
        }
    }
    return nullptr;
}

Slot* find_slot_by_identifier(std::map<std::string, Slot>& slots, std::string_view identifier) {
    for (auto& [name, slot] : slots) {
        if (slot.matches_identifier(identifier)) {
            return &slot;
        }
    }
    return nullptr;
}

const Slot* find_slot_by_identifier(const std::map<std::string, Slot>& slots,
                                    std::string_view identifier) {
    for (const auto& [name, slot] : slots) {
        if (slot.matches_identifier(identifier)) {
            return &slot;
        }
    }
    return nullptr;
}

Slot* find_other_slot(std::map<std::string, Slot>& slots, const Slot& reference) {
    for (auto& [name, slot] : slots) {
        if (slot.is_alternate_of(reference) && slot.is_writable_target()) {
            return &slot;
        }
    }
    return nullptr;
}

const Slot* find_other_slot(const std::map<std::string, Slot>& slots, const Slot& reference) {
    for (const auto& [name, slot] : slots) {
        if (slot.is_alternate_of(reference) && slot.is_writable_target()) {
            return &slot;
        }
    }
    return nullptr;
}

std::map<std::string, Slot*> get_target_group(std::map<std::string, Slot>& slots,
                                              const std::string& booted_slot_name) {
    std::map<std::string, Slot*> targets;

    Slot* booted = find_slot_by_identifier(slots, booted_slot_name);
    if (!booted) {
        LOG_WARNING("No booted slot found for target selection");
        return targets;
    }

    for (auto& [name, slot] : slots) {
        if (!slot.is_writable_target() || slot.is_booted) {
            continue;
        }

        if (slot.is_alternate_of(*booted)) {
            targets[slot.slot_class] = &slot;
        }
    }

    for (auto& [name, slot] : slots) {
        if (!slot.is_writable_target() || slot.is_booted) {
            continue;
        }

        if (targets.find(slot.slot_class) == targets.end()) {
            targets[slot.slot_class] = &slot;
        }
    }

    return targets;
}

} // namespace aegis
