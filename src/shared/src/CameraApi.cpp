#include "CameraApi.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "env.h"
#include "logging.h"

namespace {
class ScopedSocket {
   public:
    explicit ScopedSocket(int fd) : fd_(fd) {
    }
    ~ScopedSocket() {
        if (fd_ >= 0)
            ::close(fd_);
    }
    int get() const {
        return fd_;
    }
    void reset(int fd = -1) {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = fd;
    }
    int release() {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }

   private:
    int fd_;
};
}  // namespace

bool CameraApi::connectToCamera(const std::string& ip, int port, int timeoutMs) {
    ScopedSocket sock(socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() < 0)
        return false;

    int flags = fcntl(sock.get(), F_GETFL, 0);
    fcntl(sock.get(), F_SETFL, flags | O_NONBLOCK);

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    int res = connect(sock.get(), (sockaddr*)&serv_addr, sizeof(serv_addr));
    if (res < 0 && errno != EINPROGRESS)
        return false;

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock.get(), &fdset);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};

    if (select(sock.get() + 1, nullptr, &fdset, nullptr, &tv) <= 0)
        return false;

    fcntl(sock.get(), F_SETFL, flags);

    sockfd = sock.release();
    return true;
}

bool CameraApi::sendAll(const char* buf, int len, int /*timeoutMs*/) {
    int totalSent = 0;
    while (totalSent < len) {
        int sent = send(sockfd, buf + totalSent, len - totalSent, 0);
        if (sent <= 0)
            return false;
        totalSent += sent;
    }
    return true;
}

void CameraApi::closeSocket() {
    if (sockfd >= 0) {
        ::close(sockfd);
        sockfd = -1;
    }
}

bool CameraApi::sendCommand(int cmd, int value, const char* data, int dataSize, int timeoutMs) {
    if (!connectToCamera(Env::CAMERA_IP, Env::CAMERA_PORT, timeoutMs))
        return false;

    int cmdLE = htole32(cmd);
    int valueLE = htole32(value);

    if (!sendAll(reinterpret_cast<const char*>(&cmdLE), sizeof(cmdLE), timeoutMs) ||
        !sendAll(reinterpret_cast<const char*>(&valueLE), sizeof(valueLE), timeoutMs) ||
        (data && dataSize > 0 && !sendAll(data, dataSize, timeoutMs))) {
        closeSocket();
        return false;
    }
    return true;
}

bool CameraApi::sendJsonData(int cmd, const nlohmann::json& jsRequest, nlohmann::json& jsResponse, int timeoutMs) {
    std::string strJson = jsRequest.dump();
    int jsonSize = strJson.size();

    if (!sendCommand(cmd, jsonSize, strJson.data(), jsonSize, timeoutMs)) {
        LOG_E("Failed to send command or JSON data");
        closeSocket();
        return false;
    }

    int respSize = 0;
    int received = recv(sockfd, reinterpret_cast<char*>(&respSize), sizeof(respSize), MSG_WAITALL);
    if (received != sizeof(respSize) || respSize < 1) {
        LOG_D("Failed to receive response size or invalid size: {}", respSize);
        closeSocket();
        return false;
    }
    respSize = le32toh(respSize);

    std::vector<char> respData(respSize + 1, 0);
    int totalReceived = 0;
    while (totalReceived < respSize) {
        int r = recv(sockfd, respData.data() + totalReceived, respSize - totalReceived, 0);
        if (r <= 0) {
            closeSocket();
            return false;
        }
        totalReceived += r;
    }
    respData[respSize] = '\0';

    try {
        jsResponse = nlohmann::json::parse(respData.data());
        LOG_D("Received JSON response: {}", jsResponse.dump(4));
    } catch (const std::exception& e) {
        LOG_E("Failed to parse JSON response: {} | Error: {}", std::string(respData.data(), respSize), e.what());
        closeSocket();
        return false;
    }

    closeSocket();
    return true;
}