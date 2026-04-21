#include "aegis/bootloader/nvboot_control.hpp"

#include <stdexcept>
#include <string>

#include "aegis/io/command_runner.hpp"
#include "aegis/common/util.hpp"

namespace aegis {

namespace {

std::string runCapture(const std::string& cmd) {
    CommandRunner runner;
    const auto result = runner.run(cmd);
    if (result.exitCode != 0) {
        throw std::runtime_error("Command failed (" + std::to_string(result.exitCode) + "): " + cmd +
                                 "\n" + result.output);
    }
    auto out = result.output;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
        out.pop_back();
    }
    return out;
}

}  // namespace

bool NVBootControl::isValidSlotName(const std::string& value) {
    return value == "A" || value == "B";
}

void NVBootControl::validateSlotName(const std::string& slot) {
    if (!isValidSlotName(slot)) {
        throw std::runtime_error("Invalid slot: " + slot);
    }
}

std::string NVBootControl::otherSlot(const std::string& slot) {
    validateSlotName(slot);
    return slot == "A" ? "B" : "A";
}

std::string NVBootControl::getBootedSlot() const {
    try {
        const auto out = runCapture("nvbootctrl get-current-slot");
        if (out == "0") return "A";
        if (out == "1") return "B";
        logWarn("Unexpected nvbootctrl get-current-slot output: '" + out + "', defaulting to A");
    } catch (const std::exception& e) {
        logWarn(std::string("nvbootctrl get-current-slot failed: ") + e.what());
    }
    return "A";
}

std::string NVBootControl::getPrimarySlot() const {
    try {
        const auto out = runCapture("nvbootctrl -t rootfs dump-slots-info");
        const std::string marker = "Active rootfs slot: ";
        const auto pos = out.find(marker);
        if (pos != std::string::npos && pos + marker.size() < out.size()) {
            const std::string slot(1, out[pos + marker.size()]);
            if (isValidSlotName(slot)) {
                logDebug("Active rootfs slot: " + slot);
                return slot;
            }
        }
        logWarn("Failed to parse active rootfs slot from: " + out);
    } catch (const std::exception& e) {
        logWarn(std::string("nvbootctrl dump-slots-info failed: ") + e.what());
    }
    return "A";
}

std::string NVBootControl::getInactiveSlot() const {
    return otherSlot(getBootedSlot());
}

bool NVBootControl::isSlotBootable(const std::string& slot) const {
    validateSlotName(slot);
    const auto num = slot == "A" ? "0" : "1";
    try {
        const auto out = runCapture("nvbootctrl -t rootfs dump-slots-info");
        const std::string slotMarker = "slot: " + std::string(num) + ",";
        const auto pos = out.find(slotMarker);
        if (pos != std::string::npos) {
            const auto lineEnd = out.find('\n', pos);
            const auto line = out.substr(pos, lineEnd == std::string::npos ? std::string::npos : lineEnd - pos);
            return line.find("status: normal") != std::string::npos;
        }
        logWarn("Could not find slot " + std::string(num) + " in dump-slots-info output");
    } catch (const std::exception& e) {
        logWarn(std::string("isSlotBootable failed: ") + e.what());
    }
    return false;
}

void NVBootControl::setSlotBootable(const std::string& slot, bool bootable) const {
    validateSlotName(slot);
    if (!bootable) {
        logWarn("setSlotBootable(false) is not supported on NVIDIA Jetson; ignored for slot " + slot);
        return;
    }
    const auto varname = std::string("RootfsStatus") + (slot == "A" ? "SlotA" : "SlotB");
    const auto cmd = "/bin/sh -c \". /usr/bin/uefi_common.func; set_efi_var '"
                     + varname + "' '" + kGuid + "' '\\x00\\x00\\x00\\x00'\"";
    runCapture(cmd);
}

void NVBootControl::setPrimarySlot(const std::string& slot) const {
    validateSlotName(slot);
    if (!isSlotBootable(slot)) {
        throw std::runtime_error("Slot is not bootable: " + slot);
    }
    const auto num = slot == "A" ? "0" : "1";
    runCapture("nvbootctrl -t rootfs set-active-boot-slot " + std::string(num));
}

void NVBootControl::markGood(const std::string& slot) const {
    validateSlotName(slot);
    setSlotBootable(slot, true);
    setPrimarySlot(slot);
}

void NVBootControl::markBad(const std::string& slot) const {
    logWarn("markBad is not supported on NVIDIA Jetson; slot " + slot +
            " will be abandoned via bootloader retry-count exhaustion");
}

}  // namespace aegis
