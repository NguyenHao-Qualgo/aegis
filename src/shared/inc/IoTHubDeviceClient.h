#pragma once

#include <azure_c_shared_utility/map.h>
#include <iothub.h>
#include <iothub_client_options.h>
#include <iothub_client_properties.h>
#include <iothub_device_client.h>
#include <iothub_message.h>
#include <iothubtransportmqtt.h>

#include <string>

#include "IoTHubInterface.h"

/*
 * Hack to use this class to connect multiple modules with only using 1 device certificate
 */

class IoTHubDeviceClient : public IoTHubInterface {
   public:
    explicit IoTHubDeviceClient();

    int PlatformInit() override;
    void PlatformDeInit() override;

    IOTHUB_DEVICE_CLIENT_HANDLE
    CreateClient(const std::string& connectionString) override;
    IOTHUB_DEVICE_CLIENT_HANDLE
    CreateClientWithX509(const std::string& iotHubHostname, const std::string& deviceId, const std::string& moduleID,
        const std::string& x509Certificate, const std::string& x509PrivateKey) override;
    void DestroyIoTHubClient(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle) override;

    void DestroyMessage(IOTHUB_MESSAGE_HANDLE messageHandle) override;
    MAP_HANDLE GetMessageProperties(IOTHUB_MESSAGE_HANDLE messageHandle) override;
    IOTHUB_MESSAGE_HANDLE
    CreateMessageFromString(const std::string& message) override;
    std::string GetMessageString(IOTHUB_MESSAGE_HANDLE messageHandle) override;

    IOTHUB_CLIENT_RESULT SendEventAsync(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle,
        IOTHUB_MESSAGE_HANDLE eventMessageHandle, IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK eventConfirmationCallback,
        void* userContextCallback) override;

    IOTHUB_CLIENT_RESULT SetInputMessageCallback(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle, const char* inputName,
        IOTHUB_CLIENT_MESSAGE_CALLBACK_ASYNC eventHandlerCallback, void* userContextCallback) override;

    IOTHUB_CLIENT_RESULT GetPropertiesAndSubscribeToUpdatesAsync(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle,
        IOTHUB_CLIENT_PROPERTIES_RECEIVED_CALLBACK propertiesCallback, void* userContextCallback) override;

    IOTHUB_CLIENT_RESULT
    GetTwinAsync(IOTHUB_DEVICE_CLIENT_HANDLE clientHandle, IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twinCallback,
        void* userContextCallback) override;

    IOTHUB_MESSAGE_RESULT SetMessageProperty(
        IOTHUB_MESSAGE_HANDLE messageHandle, const std::string& key, const std::string& value) override;

    IOTHUB_CLIENT_RESULT SetConnectionStatusCallback(IOTHUB_DEVICE_CLIENT_HANDLE IoTHubDeviceClientHandle,
        IOTHUB_CLIENT_CONNECTION_STATUS_CALLBACK connectionStatusCallback, void* userContextCallback) override;

    IOTHUB_MESSAGE_RESULT
    SetMessageOutputName(IOTHUB_MESSAGE_HANDLE message_handle, const std::string& output_name) override;

    IOTHUB_CLIENT_RESULT SubscribeToDirectMethodCommands(IOTHUB_DEVICE_CLIENT_HANDLE iot_hub_module_client_handle,
        IOTHUB_CLIENT_COMMAND_CALLBACK_ASYNC command_callback, void* user_context_callback) override;

    IOTHUB_CLIENT_RESULT SendReportedState(IOTHUB_DEVICE_CLIENT_HANDLE iot_hub_module_client_handle,
        const void* reportedState, size_t size, IOTHUB_CLIENT_REPORTED_STATE_CALLBACK propertyAcknowledgedCallback,
        void* userContextCallback) override;
};