#pragma once

#include <nlohmann/json.hpp>
#include <string>

class EventInterface {
   public:
    virtual ~EventInterface() = default;
    virtual bool fromJson(const nlohmann::json& j) = 0;
    virtual nlohmann::json toJson() const = 0;

   protected:
    std::chrono::milliseconds getCurrentTimestamp() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
    }
    static std::string toString(const nlohmann::json& val);
    static std::string toString(const bool val);
    static std::string toString(const int val);
    static std::string toString(const double val);
};

inline std::string EventInterface::toString(const nlohmann::json& val) {
    return val.dump();
}

inline std::string EventInterface::toString(const bool val) {
    return val ? "true" : "false";
}

inline std::string EventInterface::toString(const int val) {
    return std::to_string(val);
}

inline std::string EventInterface::toString(const double val) {
    return std::to_string(val);
}
