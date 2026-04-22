#pragma once

#include <boost/asio.hpp>
#include <optional>
#include <string>

class IPCClient {
   public:
    // Construct with UNIX domain socket path
    explicit IPCClient(const std::string& socket_path);

    std::optional<std::string> get(const std::string& endpoint);

    std::optional<std::string> post(const std::string& endpoint, const std::string& body);

    void set_socket_path(const std::string& socket_path);
    std::string get_socket_path() const;

   private:
    std::optional<std::string> send_request(const std::string& request);

    std::string socket_path_;
    boost::asio::io_context io_context_;
};