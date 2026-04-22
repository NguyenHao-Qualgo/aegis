#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

class IPCServer {
   public:
    using RequestHandler = std::function<nlohmann::json(const std::string&)>;

    IPCServer(const std::string& socket_path, RequestHandler handler);

    ~IPCServer();
    void run();

    void stop();

   private:
    std::string socket_path_;
    boost::asio::io_context io_context_;
    boost::asio::local::stream_protocol::acceptor acceptor_;
    RequestHandler handler_;
    bool running_;
};