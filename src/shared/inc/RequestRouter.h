#pragma once
#include <memory>
#include <vector>

#include "RequestHandler.h"
#include "logging.h"

class RequestRouter : public RequestHandler {
   public:
    void add_handler(std::unique_ptr<RequestHandler> handler) {
        handlers_.emplace_back(std::move(handler));
    }

    bool can_handle(const std::string&) const override {
        return true;
    }

    nlohmann::json handle(const std::string& request) override {
        LOG_D(">>>>> Request: {}", request);
        for (const auto& handler : handlers_) {
            if (handler->can_handle(request)) {
                return handler->handle(request);
            }
        }
        return {{"code", 404}, {"data", {{"error", "unknown endpoint"}}}};
    }

   private:
    std::vector<std::unique_ptr<RequestHandler>> handlers_;
};