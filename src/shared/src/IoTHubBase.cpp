#include "IoTHubBase.h"

#include <iostream>

#include "ModuleConfig.h"
#include "env.h"
#include "logging.h"

IoTHubBase::IoTHubBase(IoTHubInterface& iot_hub, const std::string& module_name)
    : _client{nullptr}, _iot_hub_if(iot_hub), _connect(false), _init(false), _module_name(module_name) {
}

IoTHubBase::~IoTHubBase() {
    if (_client) {
        _iot_hub_if.DestroyIoTHubClient(_client);
        _client = nullptr;
    }
}

IOTHUB_CLIENT_CORE_HANDLE IoTHubBase::GetClient() {
    if (_client) {
        return _client;
    }
    if (!_init) {
        _init = _iot_hub_if.PlatformInit() == 0;
    }
    _client = createClient();
    if (_client && _iot_hub_if.SetConnectionStatusCallback(
                       _client,
                       [](IOTHUB_CLIENT_CONNECTION_STATUS status, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                           void* userContextCallback) {
                           if (!userContextCallback) {
                               LOG_W(
                                   "IoTHubBase: Connection status callback user context is "
                                   "null");
                               return;
                           }

                           auto hub = (IoTHubBase*)userContextCallback;
                           if (hub->cb_)
                               hub->cb_(status, reason);
                           if (status == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED) {
                               hub->_connect = true;
                               LOG_I("IoT Hub connection established successfully");
                               return;
                           }
                           hub->_connect = false;
                           LOG_W("IoT Hub connection failed with status: {}, reason: {}",
                               MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS, status),
                               MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS_REASON, reason));
                       },
                       this) != IOTHUB_CLIENT_OK) {
        LOG_E("IoTHubBase: Failed to set connection status callback");
    }
    return _client;
}

IOTHUB_CLIENT_CORE_HANDLE IoTHubBase::ReCreateClient() {
    if (_client) {
        _iot_hub_if.DestroyIoTHubClient(_client);
        _client = nullptr;
    }
    _connect = false;
    return GetClient();
}

IOTHUB_CLIENT_CORE_HANDLE IoTHubBase::createClient() {
    const std::string cert_path = Env::_ssl_certificate;
    const std::string ref_key_path = Env::_ssl_priv_key;
    std::string host_name = "";
    std::string device_id = "";
    ModuleConfig::GetInstance().GetProperty("host_name", host_name, {});
    ModuleConfig::GetInstance().GetProperty("device_id", device_id, {});
    if (!cert_path.empty() && !ref_key_path.empty()) {
        _connection_type = ConnectionType::X509;
        return _iot_hub_if.CreateClientWithX509(host_name, device_id, _module_name, cert_path, ref_key_path);
    }
    std::string connectionString = "";
    ModuleConfig::GetInstance().GetProperty("IotHubConnectionString", connectionString, {});
    if (!connectionString.empty()) {
        _connection_type = ConnectionType::Symmetric;
        return _iot_hub_if.CreateClient(connectionString);
    }
    return _client;
}

void IoTHubBase::OnConnectionChanged(
    std::function<void(const IOTHUB_CLIENT_CONNECTION_STATUS&, const IOTHUB_CLIENT_CONNECTION_STATUS_REASON&)>
        conn_cb) {
    cb_ = std::move(conn_cb);
}