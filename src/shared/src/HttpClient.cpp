#include "HttpClient.h"

#include "HttpStatusCode.h"
#include "logging.h"

CommandResponse HttpClient::get(
    const std::string& url, const std::string& path, uint8_t retries, cpr::Timeout timeout, std::chrono::milliseconds sleep) {
    for (uint i = 0; i < retries; i++) {
        auto res = cpr::Get(cpr::Url(url + path), timeout);
        if (res.status_code > 0) {
            CommandResponse result = {HttpStatusCode(res.status_code), res.text};
            LOG_I("GET {} <- [code: {}]", url + path, static_cast<int>(result.code));
            return result;
        }
        LOG_W("Retry {} GET {}", i, url + path);
        std::this_thread::sleep_for(sleep);
    }
    return {HttpStatusCode::RequestTimeout};
}

CommandResponse HttpClient::post(
    const std::string& url, const std::string& path, const std::string& body, uint8_t retries, cpr::Timeout timeout) {
    for (uint i = 0; i < retries; i++) {
        auto res = cpr::Post(cpr::Url(url + path), cpr::Body{body}, timeout);
        if (res.status_code > 0) {
            CommandResponse result = {HttpStatusCode(res.status_code), res.text};
            LOG_I("POST {} <- [code: {}]", url + path, static_cast<int>(result.code));
            return result;
        }
        LOG_W("Retry {} POST {}", i, url + path);
        std::this_thread::sleep_for(100ms);
    }
    return {HttpStatusCode::RequestTimeout};
}