#pragma once

#include <azure_c_shared_utility/map.h>
#include <iothub.h>
#include <iothub_client_options.h>
#include <iothub_client_properties.h>
#include <iothub_message.h>
#include <iothub_module_client.h>
#include <iothubtransportmqtt.h>

#include <string>

class IoTHubInterface {
   public:
    virtual int PlatformInit() = 0;
    virtual void PlatformDeInit() = 0;
    virtual IOTHUB_CLIENT_CORE_HANDLE CreateClient(const std::string& connectionString) = 0;
    virtual IOTHUB_CLIENT_CORE_HANDLE CreateClientWithX509(const std::string& iotHubHostname,
        const std::string& deviceId, const std::string& moduleID, const std::string& x509Certificate,
        const std::string& x509PrivateKey) = 0;

    virtual void DestroyIoTHubClient(IOTHUB_CLIENT_CORE_HANDLE clientHandle) = 0;
    virtual void DestroyMessage(IOTHUB_MESSAGE_HANDLE messageHandle) = 0;
    virtual MAP_HANDLE GetMessageProperties(IOTHUB_MESSAGE_HANDLE messageHandle) = 0;
    virtual IOTHUB_MESSAGE_HANDLE CreateMessageFromString(const std::string& message) = 0;
    virtual std::string GetMessageString(IOTHUB_MESSAGE_HANDLE messageHandle) = 0;

    virtual IOTHUB_CLIENT_RESULT SendEventAsync(IOTHUB_CLIENT_CORE_HANDLE clientHandle,
        IOTHUB_MESSAGE_HANDLE eventMessageHandle, IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK eventConfirmationCallback,
        void* userContextCallback) = 0;

    // virtual IOTHUB_CLIENT_RESULT SendEventAsync(
    //     IOTHUB_CLIENT_CORE_HANDLE clientHandle,
    //     IOTHUB_MESSAGE_HANDLE eventMessageHandle,
    //     IOTHUB_CLIENT_EVENT_CONFIRMATION_CALLBACK eventConfirmationCallback,
    //     void *userContextCallback, const char* outputName) = 0;

    virtual IOTHUB_CLIENT_RESULT SetInputMessageCallback(IOTHUB_CLIENT_CORE_HANDLE clientHandle, const char* inputName,
        IOTHUB_CLIENT_MESSAGE_CALLBACK_ASYNC eventHandlerCallback, void* userContextCallback) = 0;

    virtual IOTHUB_CLIENT_RESULT GetPropertiesAndSubscribeToUpdatesAsync(IOTHUB_CLIENT_CORE_HANDLE clientHandle,
        IOTHUB_CLIENT_PROPERTIES_RECEIVED_CALLBACK propertiesCallback, void* userContextCallback) = 0;

    virtual IOTHUB_CLIENT_RESULT GetTwinAsync(IOTHUB_CLIENT_CORE_HANDLE clientHandle,
        IOTHUB_CLIENT_DEVICE_TWIN_CALLBACK twinCallback, void* userContextCallback) = 0;

    virtual IOTHUB_MESSAGE_RESULT SetMessageProperty(
        IOTHUB_MESSAGE_HANDLE messageHandle, const std::string& key, const std::string& value) = 0;

    virtual IOTHUB_CLIENT_RESULT SetConnectionStatusCallback(IOTHUB_CLIENT_CORE_HANDLE clientHandle,
        IOTHUB_CLIENT_CONNECTION_STATUS_CALLBACK connectionStatusCallback, void* userContextCallback) = 0;

    virtual IOTHUB_MESSAGE_RESULT SetMessageOutputName(
        IOTHUB_MESSAGE_HANDLE message_handle, const std::string& output_name) = 0;

    virtual IOTHUB_CLIENT_RESULT SubscribeToDirectMethodCommands(IOTHUB_CLIENT_CORE_HANDLE clientHandle,
        IOTHUB_CLIENT_COMMAND_CALLBACK_ASYNC command_callback, void* user_context_callback) = 0;

    virtual IOTHUB_CLIENT_RESULT SendReportedState(IOTHUB_CLIENT_CORE_HANDLE clientHandle, const void* reportedState,
        size_t size, IOTHUB_CLIENT_REPORTED_STATE_CALLBACK propertyAcknowledgedCallback, void* userContextCallback) = 0;
};