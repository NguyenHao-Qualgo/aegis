#pragma once

#include "IoTHubBase.h"

class IoTHubSender {
    IoTHubBase& _iotHubBase;
    IoTHubInterface& _iotHub_if;

   private:
    bool SendMessageEvent(IOTHUB_MESSAGE_HANDLE& messageHandle);
    static void MessageConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT, void*);

   public:
    explicit IoTHubSender(IoTHubBase& iotHubBase, IoTHubInterface& iotHub);
    bool SendDiscoveryMessage(const std::string& msg, const std::string& device_type, const std::string& msg_type);
    bool SendCameraPresenceMessage(const std::string& msg, const std::string& device_type, const std::string& msg_type);
    bool SendFaultMessage(const std::string& msg, const std::string& device_type, const std::string& msg_type);
    bool SendMessageToIoTHub(const std::string& msg, const std::string& device_type, const std::string& msg_type);
    static void ReportedStateCallback(int status, void* userContextCallback);
    bool SendReportState(const std::string& reportedStateJson);
    ~IoTHubSender() = default;
};