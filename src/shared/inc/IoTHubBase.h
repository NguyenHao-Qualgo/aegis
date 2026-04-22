#pragma once

#include <functional>

#include "IoTHubInterface.h"

enum class ConnectionType { Unkown, Symmetric, X509, TPM };

class IoTHubBase {
    IOTHUB_CLIENT_CORE_HANDLE _client;
    IoTHubInterface& _iot_hub_if;
    bool _connect;
    bool _init;
    std::string _module_name;

   public:
    explicit IoTHubBase(IoTHubInterface& iot_hub, const std::string& module_name = "default");
    ~IoTHubBase();
    IOTHUB_CLIENT_CORE_HANDLE GetClient();
    IOTHUB_CLIENT_CORE_HANDLE ReCreateClient();
    bool IsConnected() const {
        return _connect;
    }
    void OnConnectionChanged(
        std::function<void(const IOTHUB_CLIENT_CONNECTION_STATUS&, const IOTHUB_CLIENT_CONNECTION_STATUS_REASON&)>
            conn_cb);

   private:
    IOTHUB_CLIENT_CORE_HANDLE createClient();
    ConnectionType getConnectionType() {
        return _connection_type;
    }
    ConnectionType _connection_type{ConnectionType::Unkown};
    std::function<void(const IOTHUB_CLIENT_CONNECTION_STATUS&, const IOTHUB_CLIENT_CONNECTION_STATUS_REASON&)> cb_;
};