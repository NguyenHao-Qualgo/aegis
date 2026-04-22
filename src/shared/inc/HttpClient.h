#pragma once
#include <cpr/cpr.h>

#include <map>
#include <string>

#include "common.h"

using namespace std::chrono_literals;

class HttpClient {
   public:
    CommandResponse get(
        const std::string& url, const std::string& path, uint8_t retries = 3, cpr::Timeout timeout = 5s, std::chrono::milliseconds sleep = 100ms);
    CommandResponse post(const std::string& url, const std::string& path, const std::string& body, uint8_t retries = 3,
        cpr::Timeout timeout = 5s);
};