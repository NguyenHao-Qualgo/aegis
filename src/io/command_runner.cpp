#include "aegis/io/command_runner.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <sys/wait.h>

namespace aegis {

CommandResult CommandRunner::run(const std::string& command) const {
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen failed");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int rc = pclose(pipe);
    CommandResult result;
    result.exitCode = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
    result.output = output;
    return result;
}

std::string CommandRunner::runOrThrow(const std::string& command) const {
    const auto result = run(command + " 2>&1");
    if (result.exitCode != 0) {
        throw std::runtime_error("Command failed: " + command + "\n" + result.output);
    }
    return result.output;
}

}  // namespace aegis
