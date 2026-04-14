#pragma once

#include "aegis/error.h"

#include <cstdint>
#include <string>

namespace aegis {

struct DmTarget {
    std::string dm_name;
    std::string dm_device;
    bool active = false;
};

struct LoopDevice {
    std::string path;  ///< e.g. /dev/loop0
    int fd = -1;       ///< keep open while in use
};

LoopDevice loop_setup(const std::string& file_path,
                      uint64_t offset = 0,
                      uint64_t size = 0);

Result<void> loop_teardown(const LoopDevice& loop);

DmTarget dm_verity_setup(const std::string& data_device,
                         uint64_t data_size,
                         const std::string& root_hash,
                         const std::string& salt,
                         uint64_t hash_offset);

DmTarget dm_crypt_setup(const std::string& data_device,
                        uint64_t data_size,
                        const std::string& hex_key,
                        const std::string& cipher = "aes-cbc-plain64");

Result<void> dm_remove(const std::string& dm_name);

} // namespace aegis