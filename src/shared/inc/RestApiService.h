#pragma once
#include <httplib.h>

#include <functional>
#include <string>
#include <thread>

class RestApiService {
   public:
    explicit RestApiService(int port);
    ~RestApiService();

    void start();
    void stop();

    void register_get(const std::string& path, httplib::Server::Handler handler);
    void register_post(const std::string& path, httplib::Server::Handler handler);

   private:
    void setup_routes();

    httplib::Server server_;
    std::thread server_thread_;
    bool running_ = false;
    int port_;
};