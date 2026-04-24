#include "aegis/common/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "aegis/common/util.hpp"

namespace aegis {
namespace {

void assign_key(OtaConfig& config, const std::string& key, const std::string& value) {
    if (key == "public-key") {
        config.public_key = value;
    } else if (key == "aes-key") {
        config.aes_key = value;
    } else if (key == "data-directory") {
        config.data_directory = value;
    } else if (key == "bootloader-type") {
        if (value == "nvidia") {
            config.bootloader_type = BootloaderType::Nvidia;
        } else {
            config.bootloader_type = BootloaderType::UBoot;
        }
    } else if (key == "hw-compatibility") {
        config.hw_compatibility = value;
    }
}

}  // namespace

OtaConfig ConfigLoader::load(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open config: " + path);
    }

    OtaConfig config;
    std::string line;
    std::string current_section;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::string stripped = trim(line);
        if (is_comment_or_empty(stripped)) {
            continue;
        }

        std::string section_name;
        if (is_section_header(stripped, section_name)) {
            current_section = section_name;
            continue;
        }

        const std::size_t eq_pos = stripped.find('=');
        if (eq_pos == std::string::npos) {
            throw std::runtime_error(
                "invalid config line " + std::to_string(line_number) + " in " + path + ": missing '='");
        }

        const std::string key = trim(stripped.substr(0, eq_pos));
        const std::string raw_value = trim(stripped.substr(eq_pos + 1));
        const std::string value = strip_quotes(raw_value);

        // Accept:
        // 1. no section at all
        // 2. [update] section
        if (current_section.empty() || current_section == "update") {
            assign_key(config, key, value);
        }
        LOG_D("Config: " + key + " = " + value);
    }

    return config;
}

}  // namespace aegis
