#pragma once

#include <functional>
#include <nlohmann/json.hpp>

#include "IoTHubInterface.h"
class IoTHubTwin {
    IoTHubInterface& _iotHub_if;

   private:
    static void propertiesCallback(IOTHUB_CLIENT_PROPERTY_PAYLOAD_TYPE payloadType, const unsigned char* payload,
        size_t size, void* userContextCallback);
    void setTwinProperties(const nlohmann::json& properties);
    std::function<void(const nlohmann::json&)> cb_;

   public:
    explicit IoTHubTwin(IoTHubInterface& iotHub);
    bool registerPropertiesCallback(IOTHUB_CLIENT_CORE_HANDLE& clientHandle);
    void OnTwinUpdated(std::function<void(const nlohmann::json&)>& update_cb);
};