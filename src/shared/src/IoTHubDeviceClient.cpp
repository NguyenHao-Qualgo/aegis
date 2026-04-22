#include "IoTHubDeviceClient.h"

#include <iostream>

#include "ModuleConfig.h"
#include "env.h"
#include "logging.h"
#include "utils.h"

IoTHubDeviceClient::IoTHubDeviceClient() {
}

int IoTHubDeviceClient::PlatformInit() {
    return IoTHub_Init();
}

void IoTHubDeviceClient::PlatformDeInit() {
    IoTHub_Deinit();
}

IOTHUB_DEVICE_CLIENT_HANDLE
IoTHubDeviceClient::CreateClient(const std::string& connectionString) {
    IOTHUB_DEVICE_CLIENT_HANDLE client =
        IoTHubDeviceClient_CreateFromConnectionString(connectionString.c_str(), MQTT_Protocol);
    if (!client) {
        LOG_W("Failed to create IoT Hub client from connection string {}", connectionString);
        return nullptr;
    }

    auto retryInterval = 0;
    ModuleConfig::GetInstance().GetProperty("RecoverIotHubSecs", retryInterval, 60);
    if (retryInterval < 1) {
        retryInterval = 60;
    }

    if (IoTHubDeviceClient_SetRetryPolicy(client, IOTHUB_CLIENT_RETRY_INTERVAL, retryInterval) != IOTHUB_CLIENT_OK) {
        LOG_W("Failed to set IoT Hub retry policy");
        IoTHubDeviceClient_Destroy(client);
        return nullptr;
    }

    if (IoTHubDeviceClient_SetRetryPolicy(client, IOTHUB_CLIENT_RETRY_EXPONENTIAL_BACKOFF, 0) != IOTHUB_CLIENT_OK) {
        LOG_W("Failed to set IoT Hub retry policy");
        IoTHubDeviceClient_Destroy(client);
        return nullptr;
    }

    auto keepAliveSecs = 0;
    ModuleConfig::GetInstance().GetProperty("KeepAliveSecs", keepAliveSecs, 240);
    if (keepAliveSecs > 0) {
        if (IoTHubDeviceClient_SetOption(client, OPTION_KEEP_ALIVE, &keepAliveSecs) != IOTHUB_CLIENT_OK) {
            LOG_W("Failed to set IoT Hub keep-alive option");
            IoTHubDeviceClient_Destroy(client);
            return nullptr;
        }
    }

    return client;
}

IOTHUB_DEVICE_CLIENT_HANDLE IoTHubDeviceClient::CreateClientWithX509(const std::string& iotHubHostname,
    const std::string& deviceId, const std::string& moduleID, const std::string& x509Certificate,
    const std::string& x509PrivateKey) {

    std::string cert = ReadFileContent(x509Certificate);
    std::string pkey = ReadFileContent(x509PrivateKey);

    if (cert.empty() || pkey.empty()) {
        LOG_W("X.509 certificate or private key is empty");
        return nullptr;
    }
    // DeviceId must have a value
    std::string connectionString =
        "HostName=" + iotHubHostname + ";DeviceId=" + deviceId;
    if (!moduleID.empty()) {
        connectionString += ";ModuleId=" + moduleID;
    }
    connectionString +=";x509=true";
    LOG_I("Creating IoT Hub client with X.509 certificate for device: {}", deviceId);
    auto client = CreateClient(connectionString);
    if (client) {
        if ((IoTHubDeviceClient_SetOption(client, OPTION_X509_CERT, cert.c_str()) != IOTHUB_CLIENT_OK) ||
            (IoTHubDeviceClient_SetOption(client, OPTION_X509_PRIVATE_KEY, pkey.c_str()) != IOTHUB_CLIENT_OK)) {
            LOG_W(
                "Failed to set X.509 certificate or private key options for IoT "
                "Hub client");
            IoTHubDeviceClient_Destroy(client);
            return nullptr;
        }
    }
    return client;
}

void IoTHubDeviceClient::DestroyMessage(IOTHUB_MESSAGE_HANDLE message_handle) {
    IoTHubMessage_Destroy(message_handle);
}

MAP_HANDLE
IoTHubDeviceClient::GetMessageProperties(IOTHUB_MESSAGE_HANDLE message_handle) {
    return IoTHubMessage_Properties(message_handle);
}

void IoTHubDeviceClient::DestroyIoTHubClient(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle) {
    IoTHubDeviceClient_Destroy(clientHandle);
}

IOTHUB_MESSAGE_HANDLE
IoTHubDeviceClient::CreateMessageFromString(const std::string& message) {
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(message.c_str());
    if (!messageHandle) {
        LOG_W("Failed to create IoTHubMessage from string: {}", message);
    }
    return messageHandle;
}

std::string IoTHubDeviceClient::GetMessageString(IOTHUB_MESSAGE_HANDLE message_handle) {
    const char* messageString = IoTHubMessage_GetString(message_handle);
    return messageString ? std::string(messageString) : std::string();
}

IOTHUB_CLIENT_RESULT IoTHubDeviceClient::SendEventAsync(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle,
    IOTHUB_MESSAGE_HANDLE eventMessageHandle, IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK eventConfirmationCallback,
    void* userContextCallback) {
    return IoTHubDeviceClient_SendEventAsync(
        clientHandle, eventMessageHandle, eventConfirmationCallback, userContextCallback);
}

IOTHUB_CLIENT_RESULT IoTHubDeviceClient::SetInputMessageCallback(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle,
    const char* inputName, IOTHUB_CLIENT_MESSAGE_CALLBACK_ASYNC eventHandlerCallback, void* userContextCallback) {
    return IoTHubDeviceClient_SetMessageCallback(clientHandle, eventHandlerCallback, userContextCallback);
}

IOTHUB_CLIENT_RESULT IoTHubDeviceClient::GetPropertiesAndSubscribeToUpdatesAsync(
    IOTHUB_DEVICE_CLIENT_HANDLE clientHandle, IOTHUB_CLIENT_PROPERTIES_RECEIVED_CALLBACK propertiesCallback,
    void* userContextCallback) {
    return IoTHubDeviceClient_GetPropertiesAndSubscribeToUpdatesAsync(
        clientHandle, propertiesCallback, userContextCallback);
}

IOTHUB_CLIENT_RESULT
IoTHubDeviceClient::GetTwinAsync(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle,
    IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twinCallback, void* userContextCallback) {
    return IoTHubDeviceClient_GetTwinAsync(clientHandle, twinCallback, userContextCallback);
}

IOTHUB_MESSAGE_RESULT
IoTHubDeviceClient::SetMessageProperty(
    IOTHUB_MESSAGE_HANDLE messageHandle, const std::string& key, const std::string& value) {
    return IoTHubMessage_SetProperty(messageHandle, key.c_str(), value.c_str());
}

IOTHUB_CLIENT_RESULT IoTHubDeviceClient::SetConnectionStatusCallback(
    IOTHUB_DEVICE_CLIENT_HANDLE IoTHubDeviceClientHandle,
    IOTHUB_CLIENT_CONNECTION_STATUS_CALLBACK connectionStatusCallback, void* userContextCallback) {
    return IoTHubDeviceClient_SetConnectionStatusCallback(
        IoTHubDeviceClientHandle, connectionStatusCallback, userContextCallback);
}

IOTHUB_MESSAGE_RESULT
IoTHubDeviceClient::SetMessageOutputName(IOTHUB_MESSAGE_HANDLE messageHandle, const std::string& output_name) {
    return IoTHubMessage_SetOutputName(messageHandle, output_name.c_str());
}

IOTHUB_CLIENT_RESULT IoTHubDeviceClient::SubscribeToDirectMethodCommands(IOTHUB_DEVICE_CLIENT_HANDLE handle,
    IOTHUB_CLIENT_COMMAND_CALLBACK_ASYNC command_callback, void* user_context_callback) {
    return IoTHubDeviceClient_SubscribeToCommands(handle, command_callback, user_context_callback);
}

IOTHUB_CLIENT_RESULT IoTHubDeviceClient::SendReportedState(IOTHUB_DEVICE_CLIENT_HANDLE handle,
    const void* reportedState, size_t dataSize, IOTHUB_CLIENT_REPORTED_STATE_CALLBACK propertyAcknowledgedCallback,
    void* userContextCallback) {
    return IoTHubDeviceClient_SendReportedState(handle, reinterpret_cast<const unsigned char*>(reportedState), dataSize,
        propertyAcknowledgedCallback, userContextCallback);
}