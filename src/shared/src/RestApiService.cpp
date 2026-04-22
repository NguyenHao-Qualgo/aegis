#include "RestApiService.h"

#include "env.h"
#include "logging.h"

RestApiService::RestApiService(int port) : port_(port) {
    setup_routes();
}

RestApiService::~RestApiService() {
    stop();
}

void RestApiService::setup_routes() {
    // No default routes; all are registered via register_get/register_post
}

void RestApiService::register_get(const std::string& path, httplib::Server::Handler handler) {
    server_.Get(path.c_str(), handler);
}

void RestApiService::register_post(const std::string& path, httplib::Server::Handler handler) {
    server_.Post(path.c_str(), handler);
}

void RestApiService::start() {
    if (running_)
        return;
    running_ = true;
    server_thread_ = std::thread([this]() {
        LOG_I("RestApiService listening on {}:{}", Env::CAMERA_IP, port_);
        server_.listen(Env::CAMERA_IP, port_);
    });
}

void RestApiService::stop() {
    if (running_) {
        server_.stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        running_ = false;
    }
}