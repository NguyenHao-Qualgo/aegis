#pragma once

#include <nlohmann/json.hpp>
#include <string>

class CameraApi {
   public:
    bool sendJsonData(int cmd, const nlohmann::json& jsRequest, nlohmann::json& jsResponse, int timeoutMs);

   private:
    bool sendCommand(int cmd, int value, const char* data, int dataSize, int timeoutMs);
    int sockfd = -1;
    bool connectToCamera(const std::string& ip, int port, int timeoutMs);
    bool sendAll(const char* buf, int len, int timeoutMs);
    void closeSocket();
};