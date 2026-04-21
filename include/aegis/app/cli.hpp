#pragma once

#include <vector>
#include <string>

namespace aegis {

class Cli {
public:
    int run(const std::vector<std::string>& args) const;
};

}  // namespace aegis
