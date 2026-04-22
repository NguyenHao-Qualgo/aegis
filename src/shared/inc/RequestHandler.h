#pragma once
#include <nlohmann/json.hpp>
#include <string>

class RequestHandler {
   public:
    virtual ~RequestHandler() = default;
    virtual bool can_handle(const std::string& request) const = 0;
    virtual nlohmann::json handle(const std::string& request) = 0;
};