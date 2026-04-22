#include "IoTHubTwin.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "logging.h"

IoTHubTwin::IoTHubTwin(IoTHubInterface& iotHub) : _iotHub_if(iotHub) {
}
bool IoTHubTwin::registerPropertiesCallback(IOTHUB_CLIENT_CORE_HANDLE& clientHandle) {
    auto result = _iotHub_if.GetPropertiesAndSubscribeToUpdatesAsync(clientHandle, propertiesCallback, this);
    if (result != IOTHUB_CLIENT_OK) {
        LOG_W("Failed to register properties callback");
        return false;
    }
    return true;
}

void IoTHubTwin::propertiesCallback(IOTHUB_CLIENT_PROPERTY_PAYLOAD_TYPE payloadType, const unsigned char* payload,
    size_t size, void* userContextCallback) {
    if (!userContextCallback) {
        LOG_W("userContextCallback is null");
        return;
    }

    auto twin = (IoTHubTwin*)userContextCallback;
    if (!payload || size == 0) {
        LOG_W("payload is null or size is zero");
        return;
    }
    try {
        std::string payloadStr = STRING_c_str(STRING_from_byte_array(payload, size));
        nlohmann::json propertiesJson = nlohmann::json::parse(payloadStr);
        twin->setTwinProperties(propertiesJson);
    } catch (const std::exception& e) {
        LOG_E("exception occurred while parsing properties: {}", e.what());
    }
}

void IoTHubTwin::setTwinProperties(const nlohmann::json& properties) {
    if (!properties.is_object()) {
        LOG_W("properties is not a valid JSON object");
        return;
    }
    try {
        nlohmann::json desiredProperties;
        if (properties.contains("desired") && properties["desired"].is_object()) {
            desiredProperties = properties["desired"];
        } else {
            desiredProperties = properties;
        }
        LOG_D("received desired properties: {}", desiredProperties.dump());
        if (cb_) {
            cb_(desiredProperties);
        }
    } catch (std::exception& e) {
        LOG_E("exception occurred while setting properties: {}", e.what());
        return;
    }
}
void IoTHubTwin::OnTwinUpdated(std::function<void(const nlohmann::json&)>& update_cb) {
    cb_ = std::move(update_cb);
}