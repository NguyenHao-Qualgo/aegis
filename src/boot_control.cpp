#include "aegis/boot_control.hpp"

#include <stdexcept>

#include "aegis/util.hpp"

namespace aegis {

namespace {
bool isValidSlotName(const std::string& value) {
    return value == "A" || value == "B";
}
}

BootControl::BootControl(OtaConfig config, CommandRunner runner)
    : config_(std::move(config)), runner_(std::move(runner)) {}

std::string BootControl::printEnv(const std::string& name) const {
    const auto cmd = "fw_printenv -n " + shellQuote(name);
    const auto result = runner_.run(cmd);
    auto value = trim(result.output);
    if (result.exitCode != 0 || value.empty()) {
        throw std::runtime_error("Missing U-Boot env: " + name);
    }
    return value;
}

void BootControl::setEnv(const std::string& name, const std::string& value) const {
    runner_.runOrThrow("fw_setenv " + shellQuote(name) + " " + shellQuote(value));
}

std::string BootControl::statusVar(const std::string& slot) const {
    if (slot == "A") return "RootAStatus";
    if (slot == "B") return "RootBStatus";
    throw std::runtime_error("Invalid slot: " + slot);
}

std::string BootControl::getBootedSlot() const {
    const auto result = runner_.run("grep -o 'rauc.slot=[AB]' /proc/cmdline | cut -d= -f2 2>/dev/null");
    const auto value = trim(result.output);
    if (value == "A" || value == "B") {
        return value;
    }
    return getPrimarySlot();
}

std::string BootControl::getPrimarySlot() const {
    try {
        const auto value = printEnv("Bootchain");
        if (isValidSlotName(value)) {
            return value;
        }
    } catch (const std::exception&) {
    }
    // First boot may happen before Bootchain is initialized in U-Boot env.
    return "A";
}

std::string BootControl::getInactiveSlot() const {
    return getBootedSlot() == "A" ? "B" : "A";
}

bool BootControl::isSlotBootable(const std::string& slot) const {
    return printEnv(statusVar(slot)) == "1";
}

void BootControl::setSlotBootable(const std::string& slot, bool bootable) const {
    setEnv(statusVar(slot), bootable ? "1" : "0");
}

void BootControl::setPrimarySlot(const std::string& slot) const {
    if (!isSlotBootable(slot)) {
        throw std::runtime_error("Slot is not bootable: " + slot);
    }
    setEnv("Bootchain", slot);
}

void BootControl::markGood(const std::string& slot) const {
    setSlotBootable(slot, true);
    setEnv("Bootchain", slot);
}

void BootControl::markBad(const std::string& slot) const {
    setSlotBootable(slot, false);
    const auto other = slot == "A" ? "B" : "A";
    if (isSlotBootable(other)) {
        setEnv("Bootchain", other);
    }
}

}  // namespace aegis
