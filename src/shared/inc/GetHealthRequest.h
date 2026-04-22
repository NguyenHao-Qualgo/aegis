#pragma once
#include <nlohmann/json.hpp>

#include "IoTHubManager.h"
#include "RequestHandler.h"

class GetHealthRequest : public RequestHandler {
   public:
    explicit GetHealthRequest(const IoTHubManager& iot_hub_manager) : _iot_hub_manager(iot_hub_manager) {
    }
    bool can_handle(const std::string& request) const override {
        return request == "GET /Health";
    }
    nlohmann::json handle(const std::string&) override {
        bool is_iot_connected = _iot_hub_manager.IsConnected();
        return {{"code", 200}, {"data", {{"IoTHubConnected", is_iot_connected}}}};
    }

   private:
    const IoTHubManager& _iot_hub_manager;
};