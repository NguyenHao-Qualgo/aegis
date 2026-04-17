#pragma once

#include <optional>
#include <string>
#include <vector>

namespace aegis {

enum class BootloaderType {
    UBoot,
    Nvidia
};

enum class SlotType {
    Ext4
};

enum class OtaState {
    Idle,
    Sync,
    Download,
    Install,
    Reboot,
    Commit,
    Failure
};

struct SlotConfig {
    std::string name;
    std::string device;
    SlotType type{SlotType::Ext4};
    std::string bootname;
};

struct OtaConfig {
    std::string compatible;
    BootloaderType bootloader{BootloaderType::UBoot};
    std::string dataDirectory;
    std::string keyringPath;
    std::vector<SlotConfig> slots;

    const SlotConfig& slotByBootname(const std::string& bootname) const;
};

struct OtaStatus {
    OtaState state{OtaState::Idle};
    std::string operation{"idle"};
    int progress{0};
    std::string message;
    std::string lastError;
    std::string bootedSlot{"A"};
    std::string primarySlot{"A"};
    std::optional<std::string> targetSlot;
    std::string bundleVersion;
};

std::string toString(BootloaderType value);
std::string toString(SlotType value);
std::string toString(OtaState value);
BootloaderType parseBootloaderType(const std::string& value);
SlotType parseSlotType(const std::string& value);

}  // namespace aegis
