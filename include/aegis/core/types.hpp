#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aegis/common/error.hpp"

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
    std::string public_key;
    std::string aes_key;
    std::string data_directory;
    BootloaderType bootloader_type{BootloaderType::Nvidia};
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

    // Runtime workflow data for state machine
    std::string bundlePath;
    std::string installPath;
};

struct InstallOptions {
    OtaConfig config;
    std::string image_path;
    std::string target_slot;
    bool verbose = true;
};

struct AesMaterial {
    std::string key_hex;
    std::string iv_hex;
};

struct ManifestEntry {
    std::string filename;
    std::string type;
    std::string compress;
    std::string device;
    std::string path;
    std::string filesystem;
    std::string sha256;
    std::string ivt;
    bool encrypted = false;
    bool preserve_attributes = false;
    bool create_destination = false;
    bool atomic_install = false;
    bool installed = false;
};

struct CpioEntry {
    std::string name;
    std::uint64_t size = 0;
    std::uint32_t checksum = 0;
};

std::string toString(BootloaderType value);
std::string toString(SlotType value);
std::string toString(OtaState value);
BootloaderType parseBootloaderType(const std::string& value);
SlotType parseSlotType(const std::string& value);

constexpr std::size_t kIoBufferSize = 1024 * 1024;
constexpr std::size_t kCpioHeaderSize = 110;
constexpr char kTrailerName[] = "TRAILER!!!";

}  // namespace aegis
