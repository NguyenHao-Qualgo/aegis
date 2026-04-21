#pragma once

#include <iostream>
#include <string>

namespace aegis {

inline void log_info(const std::string &message) {
    std::cerr << "[cpp-swupdate] " << message << '\n';
}

inline bool &stream_logging_enabled() {
    static bool enabled = false;
    return enabled;
}

inline void set_stream_logging(bool enabled) {
    stream_logging_enabled() = enabled;
}

inline void log_stream(const std::string &message) {
    if (stream_logging_enabled()) {
        log_info(message);
    }
}

}  // namespace aegis
