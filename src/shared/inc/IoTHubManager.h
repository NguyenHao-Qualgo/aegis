#pragma once
#include <functional>

#include "IoTHubBase.h"
#include "IoTHubDeviceClient.h"
#include "IoTHubDirectMethod.h"
#include "IoTHubInterface.h"
#include "IoTHubSender.h"
#include "IoTHubTwin.h"

class IoTHubManager {
   private:
    IoTHubDeviceClient _iot_hub_interface;
    IoTHubBase _iot_hub_base;
    IoTHubSender _iot_hub_sender;
    IoTHubTwin _iot_hub_twin;
    IoTHubDirectMethod _iot_hub_direct_method;

    bool _is_connected = false;

   public:
    IoTHubManager(const std::string& module_name = "default");
    ~IoTHubManager();
    bool IsConnected() const;
    void OnTwinUpdated(std::function<void(const nlohmann::json&)> update_cb);
    void OnDirectMethodReceived(std::function<CommandResponse(const std::string&, const std::string&)> method_cb);
    void OnConnectionChanged(
        std::function<void(const IOTHUB_CLIENT_CONNECTION_STATUS&, const IOTHUB_CLIENT_CONNECTION_STATUS_REASON&)>
            conn_cb);
    IoTHubSender getSender();
    bool InitModules();
};