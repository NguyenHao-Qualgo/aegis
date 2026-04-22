#include "IPCServer.h"

#include <boost/asio/local/stream_protocol.hpp>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>

IPCServer::IPCServer(const std::string& socket_path, RequestHandler handler)
    : socket_path_(socket_path),
      acceptor_(io_context_, boost::asio::local::stream_protocol::endpoint(socket_path_)),
      handler_(std::move(handler)),
      running_(false) {
}

void IPCServer::run() {
    running_ = true;
    std::cout << "IPCServer listening on " << socket_path_ << std::endl;
    while (running_) {
        boost::asio::local::stream_protocol::socket socket(io_context_);
        boost::system::error_code ec;
        acceptor_.accept(socket, ec);

        if (!running_)
            break;

        if (ec) {
            if (!running_)
                break;
            continue;
        }

        char data[2048];
        size_t len = socket.read_some(boost::asio::buffer(data), ec);
        std::string response;
        if (!ec && len > 0) {
            std::string request(data, len);
            try {
                response = handler_(request);
            } catch (const std::exception& ex) {
                nlohmann::json err = {
                    {"code", 500}, {"data", {{"error", "Internal server error"}, {"detail", ex.what()}}}};
                response = err.dump();
            } catch (...) {
                nlohmann::json err = {{"code", 500}, {"data", {{"error", "Internal server error"}}}};
                response = err.dump();
            }
        } else {
            nlohmann::json err = {{"code", 400}, {"data", {{"error", "Bad request or read error"}}}};
            response = err.dump();
        }
        boost::asio::write(socket, boost::asio::buffer(response), ec);
        socket.close();
    }
    std::filesystem::remove(socket_path_);
}

void IPCServer::stop() {
    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);
    try {
        boost::asio::local::stream_protocol::socket sock(io_context_);
        sock.connect(boost::asio::local::stream_protocol::endpoint(socket_path_), ec);
        sock.close();
    } catch (...) {
    }
    io_context_.stop();
}

IPCServer::~IPCServer() {
    stop();
}