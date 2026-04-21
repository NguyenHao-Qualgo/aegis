#pragma once

#include <string>

#include "aegis/core/types.hpp"

namespace aegis {

class ConfigLoader {
public:
    OtaConfig load(const std::string& path);
};

}  // namespace aegis
