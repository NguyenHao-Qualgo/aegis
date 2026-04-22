#include "IoTHubManager.h"

#include <iostream>

#include "logging.h"

IoTHubManager::IoTHubManager(const std::string& module_name)
    : _iot_hub_interface(IoTHubDeviceClient()),
      _iot_hub_base(_iot_hub_interface, module_name),
      _iot_hub_sender(_iot_hub_base, _iot_hub_interface),
      _iot_hub_twin(_iot_hub_interface),
      _iot_hub_direct_method(_iot_hub_interface),
      _is_connected(false) {
}

IoTHubManager::~IoTHubManager() {
    _iot_hub_interface.PlatformDeInit();
}

void IoTHubManager::OnTwinUpdated(std::function<void(const nlohmann::json&)> update_cb) {
    _iot_hub_twin.OnTwinUpdated(update_cb);
}

void IoTHubManager::OnDirectMethodReceived(
    std::function<CommandResponse(const std::string&, const std::string&)> method_cb) {
    _iot_hub_direct_method.OnMethodReceived(method_cb);
}

void IoTHubManager::OnConnectionChanged(
    std::function<void(const IOTHUB_CLIENT_CONNECTION_STATUS&, const IOTHUB_CLIENT_CONNECTION_STATUS_REASON&)>
        conn_cb) {
    _iot_hub_base.OnConnectionChanged(conn_cb);
}

bool IoTHubManager::IsConnected() const {
    return _is_connected && _iot_hub_base.IsConnected();
}

bool IoTHubManager::InitModules() {
    auto client = _iot_hub_base.GetClient();
    if (!client) {
        LOG_W("IoT Hub client is not initialized.");
        return false;
    }
    auto twin = _iot_hub_twin.registerPropertiesCallback(client);
    auto direct_method = _iot_hub_direct_method.Register(client);
    _is_connected = twin && direct_method;
    if (_is_connected) {
        LOG_I("Modules registered successfully.");
    } else {
        LOG_W("Failed to register modules.");
    }
    return _is_connected;
}

IoTHubSender IoTHubManager::getSender() {
    return _iot_hub_sender;
}