#include "aegis/types.hpp"

#include <stdexcept>

namespace aegis {

const SlotConfig& OtaConfig::slotByBootname(const std::string& bootname) const {
    for (const auto& slot : slots) {
        if (slot.bootname == bootname) {
            return slot;
        }
    }
    throw std::runtime_error("Unknown slot bootname: " + bootname);
}

std::string toString(BootloaderType value) {
    switch (value) {
        case BootloaderType::UBoot: return "uboot";
        case BootloaderType::Nvidia: return "nvidia";
    }
    return "unknown";
}

std::string toString(SlotType value) {
    switch (value) {
        case SlotType::Ext4: return "ext4";
    }
    return "unknown";
}

std::string toString(OtaState value) {
    switch (value) {
        case OtaState::Idle: return "Idle";
        case OtaState::Download: return "Download";
        case OtaState::Install: return "Install";
        case OtaState::Reboot: return "Reboot";
        case OtaState::Commit: return "Commit";
        case OtaState::Failure: return "Failure";
    }
    return "Unknown";
}

BootloaderType parseBootloaderType(const std::string& value) {
    if (value == "uboot") return BootloaderType::UBoot;
    if (value == "nvidia") return BootloaderType::Nvidia;
    throw std::runtime_error("Unsupported bootloader: " + value);
}

SlotType parseSlotType(const std::string& value) {
    if (value == "ext4") return SlotType::Ext4;
    throw std::runtime_error("Unsupported slot type: " + value);
}

}  // namespace aegis
