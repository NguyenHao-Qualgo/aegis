#include "aegis/config.hpp"

#include <fstream>
#include <stdexcept>

#include "aegis/util.hpp"

namespace aegis {

OtaConfig ConfigLoader::load(const std::string& path) const {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Cannot open config: " + path);
    }

    OtaConfig config;
    std::string line;
    std::string section;
    SlotConfig currentSlot;
    bool inSlot = false;

    auto flushSlot = [&]() {
        if (inSlot) {
            config.slots.push_back(currentSlot);
            currentSlot = {};
            inSlot = false;
        }
    };

    while (std::getline(ifs, line)) {
        line = trim(line);
        if (line.empty() || startsWith(line, "#") || startsWith(line, ";")) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            flushSlot();
            section = line.substr(1, line.size() - 2);
            if (startsWith(section, "slot.rootfs.")) {
                inSlot = true;
                currentSlot.name = section.substr(5);
            }
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const auto key = trim(line.substr(0, pos));
        const auto value = trim(line.substr(pos + 1));

        if (section == "system") {
            if (key == "compatible") config.compatible = value;
            else if (key == "bootloader") config.bootloader = parseBootloaderType(value);
            else if (key == "data-directory") config.dataDirectory = value;
        } else if (section == "keyring") {
            if (key == "path") config.keyringPath = value;
        } else if (startsWith(section, "slot.rootfs.")) {
            if (key == "device") currentSlot.device = value;
            else if (key == "type") currentSlot.type = parseSlotType(value);
            else if (key == "bootname") currentSlot.bootname = value;
        }
    }
    flushSlot();

    if (config.compatible.empty()) throw std::runtime_error("Missing system.compatible");
    if (config.dataDirectory.empty()) throw std::runtime_error("Missing system.data-directory");
    if (config.slots.size() != 2) throw std::runtime_error("Exactly 2 rootfs slots are required");
    config.slotByBootname("A");
    config.slotByBootname("B");
    return config;
}

}  // namespace aegis
