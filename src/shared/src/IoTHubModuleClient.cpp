#include "IoTHubModuleClient.h"

#include <iostream>

#include "ModuleConfig.h"
#include "env.h"
#include "logging.h"
#include "utils.h"

IoTHubModuleClient::IoTHubModuleClient() {
}

int IoTHubModuleClient::PlatformInit() {
    return IoTHub_Init();
}

void IoTHubModuleClient::PlatformDeInit() {
    IoTHub_Deinit();
}

IOTHUB_MODULE_CLIENT_HANDLE
IoTHubModuleClient::CreateClient(const std::string& connectionString) {
    IOTHUB_MODULE_CLIENT_HANDLE client =
        IoTHubModuleClient_CreateFromConnectionString(connectionString.c_str(), MQTT_Protocol);
    if (!client) {
        LOG_W("Failed to create IoT Hub client from connection string {}", connectionString);
        return nullptr;
    }

    auto retryInterval = 0;
    ModuleConfig::GetInstance().GetProperty("RecoverIotHubSecs", retryInterval, 60);
    if (retryInterval < 1) {
        retryInterval = 60;
    }

    LOG_I(
        "Setting IoT Hub retry policy to IOTHUB_CLIENT_RETRY_INTERVAL with "
        "interval: {} seconds",
        retryInterval);
    if (IoTHubModuleClient_SetRetryPolicy(client, IOTHUB_CLIENT_RETRY_INTERVAL, retryInterval) != IOTHUB_CLIENT_OK) {
        LOG_W("Failed to set IoT Hub retry policy");
        IoTHubModuleClient_Destroy(client);
        return nullptr;
    }

    if (IoTHubModuleClient_SetRetryPolicy(client, IOTHUB_CLIENT_RETRY_EXPONENTIAL_BACKOFF, 0) != IOTHUB_CLIENT_OK) {
        LOG_W("Failed to set IoT Hub retry policy");
        IoTHubModuleClient_Destroy(client);
        return nullptr;
    }

    auto keepAliveSecs = 0;
    ModuleConfig::GetInstance().GetProperty("KeepAliveSecs", keepAliveSecs, 240);
    if (keepAliveSecs > 0) {
        LOG_I("Setting IoT Hub keep-alive interval to {} seconds", keepAliveSecs);
        if (IoTHubModuleClient_SetOption(client, OPTION_KEEP_ALIVE, &keepAliveSecs) != IOTHUB_CLIENT_OK) {
            LOG_W("Failed to set IoT Hub keep-alive option");
            IoTHubModuleClient_Destroy(client);
            return nullptr;
        }
    }

    return client;
}

IOTHUB_MODULE_CLIENT_HANDLE IoTHubModuleClient::CreateClientWithX509(const std::string& iotHubHostname,
    const std::string& deviceId, const std::string& moduleID, const std::string& x509Certificate,
    const std::string& x509PrivateKey) {

    std::string cert = ReadFileContent(x509Certificate);
    std::string pkey = ReadFileContent(x509PrivateKey);

    if (cert.empty() || pkey.empty()) {
        LOG_W("X.509 certificate or private key is empty");
        return nullptr;
    }
    std::string connectionString =
        "HostName=" + iotHubHostname + ";DeviceId=" + deviceId;
    if (!moduleID.empty()) {
        connectionString += ";ModuleId=" + moduleID;
    }
    connectionString += ";x509=true";
    LOG_I("Creating IoT Hub client with X.509 certificate for device: {}", deviceId);
    auto client = CreateClient(connectionString);
    if (client) {
        if ((IoTHubModuleClient_SetOption(client, OPTION_X509_CERT, cert.c_str()) != IOTHUB_CLIENT_OK) ||
            (IoTHubModuleClient_SetOption(client, OPTION_X509_PRIVATE_KEY, pkey.c_str()) != IOTHUB_CLIENT_OK)) {
            LOG_W(
                "Failed to set X.509 certificate or private key options for IoT "
                "Hub client");
            IoTHubModuleClient_Destroy(client);
            return nullptr;
        }
    }
    return client;
}

void IoTHubModuleClient::DestroyMessage(IOTHUB_MESSAGE_HANDLE message_handle) {
    IoTHubMessage_Destroy(message_handle);
}

MAP_HANDLE
IoTHubModuleClient::GetMessageProperties(IOTHUB_MESSAGE_HANDLE message_handle) {
    return IoTHubMessage_Properties(message_handle);
}

void IoTHubModuleClient::DestroyIoTHubClient(IOTHUB_MODULE_CLIENT_HANDLE clientHandle) {
    IoTHubModuleClient_Destroy(clientHandle);
}

IOTHUB_MESSAGE_HANDLE
IoTHubModuleClient::CreateMessageFromString(const std::string& message) {
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(message.c_str());
    if (!messageHandle) {
        LOG_W("Failed to create IoTHubMessage from string: {}", message);
    }
    return messageHandle;
}

std::string IoTHubModuleClient::GetMessageString(IOTHUB_MESSAGE_HANDLE message_handle) {
    const char* messageString = IoTHubMessage_GetString(message_handle);
    return messageString ? std::string(messageString) : std::string();
}

IOTHUB_CLIENT_RESULT IoTHubModuleClient::SendEventAsync(IOTHUB_MODULE_CLIENT_HANDLE clientHandle,
    IOTHUB_MESSAGE_HANDLE eventMessageHandle, IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK eventConfirmationCallback,
    void* userContextCallback) {
    return IoTHubModuleClient_SendEventAsync(
        clientHandle, eventMessageHandle, eventConfirmationCallback, userContextCallback);
}

IOTHUB_CLIENT_RESULT IoTHubModuleClient::SetInputMessageCallback(IOTHUB_MODULE_CLIENT_HANDLE clientHandle,
    const char* inputName, IOTHUB_CLIENT_MESSAGE_CALLBACK_ASYNC eventHandlerCallback, void* userContextCallback) {
    return IoTHubModuleClient_SetMessageCallback(clientHandle, eventHandlerCallback, userContextCallback);
}

IOTHUB_CLIENT_RESULT IoTHubModuleClient::GetPropertiesAndSubscribeToUpdatesAsync(
    IOTHUB_MODULE_CLIENT_HANDLE clientHandle, IOTHUB_CLIENT_PROPERTIES_RECEIVED_CALLBACK propertiesCallback,
    void* userContextCallback) {
    return IoTHubModuleClient_GetPropertiesAndSubscribeToUpdatesAsync(
        clientHandle, propertiesCallback, userContextCallback);
}

IOTHUB_CLIENT_RESULT
IoTHubModuleClient::GetTwinAsync(IOTHUB_MODULE_CLIENT_HANDLE clientHandle,
    IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twinCallback, void* userContextCallback) {
    return IoTHubModuleClient_GetTwinAsync(clientHandle, twinCallback, userContextCallback);
}

IOTHUB_MESSAGE_RESULT
IoTHubModuleClient::SetMessageProperty(
    IOTHUB_MESSAGE_HANDLE messageHandle, const std::string& key, const std::string& value) {
    return IoTHubMessage_SetProperty(messageHandle, key.c_str(), value.c_str());
}

IOTHUB_CLIENT_RESULT IoTHubModuleClient::SetConnectionStatusCallback(
    IOTHUB_MODULE_CLIENT_HANDLE iotHubModuleClientHandle,
    IOTHUB_CLIENT_CONNECTION_STATUS_CALLBACK connectionStatusCallback, void* userContextCallback) {
    return IoTHubModuleClient_SetConnectionStatusCallback(
        iotHubModuleClientHandle, connectionStatusCallback, userContextCallback);
}

IOTHUB_MESSAGE_RESULT
IoTHubModuleClient::SetMessageOutputName(IOTHUB_MESSAGE_HANDLE messageHandle, const std::string& output_name) {
    return IoTHubMessage_SetOutputName(messageHandle, output_name.c_str());
}

IOTHUB_CLIENT_RESULT IoTHubModuleClient::SubscribeToDirectMethodCommands(
    IOTHUB_MODULE_CLIENT_HANDLE iot_hub_module_client_handle, IOTHUB_CLIENT_COMMAND_CALLBACK_ASYNC command_callback,
    void* user_context_callback) {
    return IoTHubModuleClient_SubscribeToCommands(
        iot_hub_module_client_handle, command_callback, user_context_callback);
}

IOTHUB_CLIENT_RESULT IoTHubModuleClient::SendReportedState(IOTHUB_MODULE_CLIENT_HANDLE iot_hub_module_client_handle,
    const void* reportedState, size_t dataSize, IOTHUB_CLIENT_REPORTED_STATE_CALLBACK propertyAcknowledgedCallback,
    void* userContextCallback) {
    return IoTHubModuleClient_SendReportedState(iot_hub_module_client_handle,
        reinterpret_cast<const unsigned char*>(reportedState), dataSize, propertyAcknowledgedCallback,
        userContextCallback);
}