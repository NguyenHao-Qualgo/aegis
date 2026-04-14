#pragma once

#include <string>

namespace aegis {

struct ProgressInfo {
    int percentage = 0;
    std::string message = "idle";
    int depth = 0;
};

} // namespace aegis