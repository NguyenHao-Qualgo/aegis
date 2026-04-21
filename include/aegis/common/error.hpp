#pragma once

#include <stdexcept>
#include <string>

namespace aegis {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void fail_runtime(const std::string& message) {
    throw Error(message);
}

}  // namespace aegis
