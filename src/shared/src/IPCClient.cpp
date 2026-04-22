#include "IPCClient.h"

#include <boost/asio/local/stream_protocol.hpp>

IPCClient::IPCClient(const std::string& socket_path) : socket_path_(socket_path) {
}

void IPCClient::set_socket_path(const std::string& socket_path) {
    socket_path_ = socket_path;
}

std::string IPCClient::get_socket_path() const {
    return socket_path_;
}

std::optional<std::string> IPCClient::get(const std::string& endpoint) {
    return send_request("GET " + endpoint);
}

std::optional<std::string> IPCClient::post(const std::string& endpoint, const std::string& body) {
    return send_request("POST " + endpoint + " " + body);
}

std::optional<std::string> IPCClient::send_request(const std::string& request) {
    try {
        boost::asio::local::stream_protocol::socket socket(io_context_);
        boost::system::error_code ec;
        socket.connect(boost::asio::local::stream_protocol::endpoint(socket_path_), ec);
        if (ec)
            return std::nullopt;

        boost::asio::write(socket, boost::asio::buffer(request), ec);
        if (ec)
            return std::nullopt;

        char reply[2048];
        size_t len = socket.read_some(boost::asio::buffer(reply), ec);
        if (ec && ec != boost::asio::error::eof)
            return std::nullopt;

        return std::string(reply, len);
    } catch (...) {
        return std::nullopt;
    }
}