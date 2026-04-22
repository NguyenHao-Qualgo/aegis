#include "IoTHubSender.h"

#include <iostream>

#include "azure_macro_utils/macro_utils.h"
#include "logging.h"
#include "uuid.h"

constexpr const char* IoTHubOutputName = "iothub";

using MessageCtx = struct MessageCtx {
    IoTHubSender* sender;
    Uuid::Uuid msg_id;
    explicit MessageCtx(IoTHubSender* s) : sender(s), msg_id(Uuid::New()) {
    }
    MessageCtx(IoTHubSender* s, const Uuid::Uuid& id) : sender(s), msg_id(id) {
    }
};

IoTHubSender::IoTHubSender(IoTHubBase& iotHubBase, IoTHubInterface& iotHub)
    : _iotHubBase(iotHubBase), _iotHub_if(iotHub) {
}

bool IoTHubSender::SendDiscoveryMessage(
    const std::string& msg, const std::string& device_type, const std::string& msg_type) {
    IOTHUB_MESSAGE_HANDLE messageHandle = _iotHub_if.CreateMessageFromString(msg);
    _iotHub_if.SetMessageProperty(messageHandle, "deviceType", device_type);
    _iotHub_if.SetMessageProperty(messageHandle, "msgType", msg_type);
    _iotHub_if.SetMessageOutputName(messageHandle, "cameradiscovery");
    return SendMessageEvent(messageHandle);
}

bool IoTHubSender::SendMessageEvent(IOTHUB_MESSAGE_HANDLE& messageHandle) {
    if (!messageHandle) {
        LOG_W("messageHandle is null");
        return false;
    }
    auto msg_ctx = new MessageCtx(this);
    IOTHUB_CLIENT_CORE_HANDLE clientHandle = _iotHubBase.GetClient();
    if (clientHandle) {
        LOG_D("Sending message with ID: {}", msg_ctx->msg_id.str());
        IOTHUB_CLIENT_RESULT result =
            _iotHub_if.SendEventAsync(clientHandle, messageHandle, MessageConfirmationCallback, msg_ctx);
        if (result == IOTHUB_CLIENT_OK)
            return true;
    }
    delete msg_ctx;
    return false;
}

void IoTHubSender::MessageConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback) {
    if (!userContextCallback) {
        LOG_W(
            "IoTHubSender::MessageConfirmationCallback: userContextCallback "
            "is null");
        return;
    }
    auto context = (MessageCtx*)userContextCallback;
    auto sender = context->sender;
    if (result != IOTHUB_CLIENT_CONFIRMATION_OK) {
        LOG_E(
            "IoTHubSender::MessageConfirmationCallback: Message ID: {} "
            "failed to send, result: {}",
            context->msg_id.str(), MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
    } else {
        LOG_D(
            "IoTHubSender::MessageConfirmationCallback: Message ID: {} sent "
            "successfully",
            context->msg_id.str());
    }
    delete context;
}

bool IoTHubSender::SendCameraPresenceMessage(
    const std::string& msg, const std::string& device_type, const std::string& msg_type) {
    IOTHUB_MESSAGE_HANDLE messageHandle = _iotHub_if.CreateMessageFromString(msg);
    _iotHub_if.SetMessageProperty(messageHandle, "deviceType", device_type);
    _iotHub_if.SetMessageProperty(messageHandle, "msgType", msg_type);
    _iotHub_if.SetMessageOutputName(messageHandle, "camerapresence");
    return SendMessageEvent(messageHandle);
}

bool IoTHubSender::SendFaultMessage(const std::string& msg, const std::string& device_type, const std::string& msg_type) {
    IOTHUB_MESSAGE_HANDLE messageHandle = _iotHub_if.CreateMessageFromString(msg);
    _iotHub_if.SetMessageProperty(messageHandle, "deviceType", device_type);
    _iotHub_if.SetMessageProperty(messageHandle, "msgType", msg_type);
    _iotHub_if.SetMessageOutputName(messageHandle, "fault-events");
    return SendMessageEvent(messageHandle);
}

bool IoTHubSender::SendMessageToIoTHub(
    const std::string& msg, const std::string& device_type, const std::string& msg_type) {
    IOTHUB_MESSAGE_HANDLE messageHandle = _iotHub_if.CreateMessageFromString(msg);
    _iotHub_if.SetMessageProperty(messageHandle, "deviceType", device_type);
    _iotHub_if.SetMessageProperty(messageHandle, "msgType", msg_type);
    _iotHub_if.SetMessageOutputName(messageHandle, IoTHubOutputName);
    return SendMessageEvent(messageHandle);
}

void IoTHubSender::ReportedStateCallback(int status, void* userContextCallback) {
    if (!userContextCallback) {
        LOG_W("userContextCallback is null");
        return;
    }
    auto context = (MessageCtx*)userContextCallback;
    LOG_I("Message ID: {} reported state result: {}", context->msg_id.str(), status);
    delete context;
}

bool IoTHubSender::SendReportState(const std::string& reportedStateJson) {
    auto msg_ctx = new MessageCtx(this);
    IOTHUB_CLIENT_CORE_HANDLE clientHandle = _iotHubBase.GetClient();
    if (clientHandle) {
        LOG_I("Sending Report State {} with ID: {}", reportedStateJson, msg_ctx->msg_id.str());
        const void* reportedState = reportedStateJson.c_str();
        size_t dataSize = reportedStateJson.size();
        IOTHUB_CLIENT_RESULT result =
            _iotHub_if.SendReportedState(clientHandle, reportedState, dataSize, ReportedStateCallback, msg_ctx);
        if (result == IOTHUB_CLIENT_OK)
            return true;
    }
    delete msg_ctx;
    return false;
}