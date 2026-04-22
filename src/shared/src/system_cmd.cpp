#include "system_cmd.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// TODO Using boost.process

int shell_exec(const std::string& cmd) {
    std::string dummy_output;
    return shell_exec(cmd, dummy_output);
}

int shell_exec(const std::string& cmd, std::string& output) {
    std::array<char, 128> buffer;
    output.clear();
    int return_code = -1;
    auto pclose_wrapper = [&return_code](FILE* file) { return_code = pclose(file); };
    {  // scope is important, have to make sure the ptr goes out of scope first
        const auto pipe = std::unique_ptr<FILE, decltype(pclose_wrapper)>(popen(cmd.c_str(), "r"), pclose_wrapper);
        if (pipe) {
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                output += buffer.data();
            }
        }
    }
    return return_code;
}
