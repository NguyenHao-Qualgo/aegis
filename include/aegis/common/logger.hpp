#pragma once

#include <string>

namespace aegis {

void logDebug(const std::string& msg);
void logInfo(const std::string& msg);
void logWarn(const std::string& msg);
void logError(const std::string& msg);

inline bool& streamLoggingEnabled() {
    static bool enabled = false;
    return enabled;
}

inline void setStreamLogging(bool enabled) {
    streamLoggingEnabled() = enabled;
}

inline void logStream(const std::string& msg) {
    if (streamLoggingEnabled()) {
        logDebug(msg);
    }
}

}  // namespace aegis
