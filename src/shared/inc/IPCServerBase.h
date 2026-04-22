#pragma once

#include <memory>
#include <string>
#include <thread>

#include "IPCServer.h"
#include "RequestRouter.h"

class IPCServerBase {
   public:
    explicit IPCServerBase(const std::string& socket_path)
        : _ipc_router(std::make_unique<RequestRouter>()),
          _ipc_server(std::make_unique<IPCServer>(
              socket_path, [this](const std::string& req) { return _ipc_router->handle(req).dump(); })) {
    }

    ~IPCServerBase() {
        if (_ipc_server)
            _ipc_server->stop();
        if (_ipc_thread.joinable())
            _ipc_thread.join();
    }

    void startIPCServer() {
        _ipc_thread = std::thread([this]() { _ipc_server->run(); });
    }

    void addIPCHandler(std::unique_ptr<RequestHandler> handler) {
        _ipc_router->add_handler(std::move(handler));
    }

   protected:
    std::unique_ptr<RequestRouter> _ipc_router;

   private:
    std::unique_ptr<IPCServer> _ipc_server;
    std::thread _ipc_thread;
};