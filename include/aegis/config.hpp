#pragma once

#include <string>

#include "aegis/types.hpp"

namespace aegis {

class ConfigLoader {
public:
    OtaConfig load(const std::string& path) const;
};

}  // namespace aegis
