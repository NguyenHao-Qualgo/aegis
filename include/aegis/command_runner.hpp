#pragma once

#include <string>

namespace aegis {

struct CommandResult {
    int exitCode{0};
    std::string output;
};

class CommandRunner {
public:
    CommandResult run(const std::string& command) const;
    std::string runOrThrow(const std::string& command) const;
};

}  // namespace aegis
