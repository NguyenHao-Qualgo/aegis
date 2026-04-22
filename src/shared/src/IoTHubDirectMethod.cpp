#include "IoTHubDirectMethod.h"

#include "HttpStatusCode.h"
#include "azure_macro_utils/macro_utils.h"
#include "logging.h"

IoTHubDirectMethod::IoTHubDirectMethod(IoTHubInterface& iot_hub_interface) : iot_hub_if_(iot_hub_interface) {
}

bool ValidateMethodRequest(const IOTHUB_CLIENT_COMMAND_REQUEST* command_request) {
    return (command_request != nullptr && command_request->commandName != nullptr &&
            command_request->commandName[0] != '\0' && command_request->payload != nullptr &&
            command_request->payloadLength > 0);
}

bool SetResponseMessagePayload(std::string& msg, IOTHUB_CLIENT_COMMAND_RESPONSE* command_response) {
    command_response->payload = (unsigned char*)calloc(1, msg.size());

    if (command_response->payload == nullptr) {
        LOG_E("Failed to allocate memory for command response payload");
        return false;
    }

    memcpy(command_response->payload, msg.c_str(), msg.size());
    command_response->payloadLength = msg.size();
    return true;
}

void IoTHubDirectMethod::OnMethodReceived(
    std::function<CommandResponse(const std::string&, const std::string&)>& method_cb) {
    method_callback_ = method_cb;
}

void IoTHubDirectMethod::methodCallback(const IOTHUB_CLIENT_COMMAND_REQUEST* command_request,
    IOTHUB_CLIENT_COMMAND_RESPONSE* command_response, void* user_context_callback) {
    if (!user_context_callback) {
        LOG_W("User context callback is null");
        return;
    }

    auto self = (IoTHubDirectMethod*)user_context_callback;
    if (!ValidateMethodRequest(command_request)) {
        LOG_W("Invalid method request received");
        command_response->statusCode = toInt(HttpStatusCode::BadRequest);
        std::string error_msg = R"({"status": 400, "description": "Command payload is null."})";
        SetResponseMessagePayload(error_msg, command_response);
        return;
    }
    try {
        auto start = steady::Now();
        std::string command_name(command_request->commandName);
        std::string payload =
            STRING_c_str(STRING_from_byte_array(command_request->payload, command_request->payloadLength));
        CommandResponse response{
            HttpStatusCode::InternalServerError, R"({"status": 500, "description": "InternalServerError"})"};
        if (self->method_callback_) {
            LOG_D("Processing direct method command: {}, payload: {}", command_name, payload);
            response = self->method_callback_(command_name, payload);
        }
        command_response->statusCode = toInt(response.code);
        command_response->structVersion = command_request->structVersion;

        if (response.response.empty()) {
            response.response = R"({"description": ")" + reasonPhrase(response.code) + R"("})";
        }
        SetResponseMessagePayload(response.response, command_response);
        LOG_I("Response to Direct Method {} in {:.2f}s, code: {}, body: {}", command_name,
            steady::SecondsFromNow(start), command_response->statusCode, response.response);
    } catch (const std::exception& e) {
        LOG_E("Exception in method callback: {}", e.what());
        std::string message = R"({"status": 500, "description": "InternalServerError"})";
        command_response->statusCode = toInt(HttpStatus::Code::InternalServerError);
        SetResponseMessagePayload(message, command_response);
    }
}

bool IoTHubDirectMethod::Register(IOTHUB_CLIENT_CORE_HANDLE& client) {
    auto res = iot_hub_if_.SubscribeToDirectMethodCommands(client, methodCallback, this);
    if (res != IOTHUB_CLIENT_OK) {
        LOG_E(
            "Failed to subscribe to direct method commands, error: {}", MU_ENUM_TO_STRING(IOTHUB_CLIENT_RESULT, res));
        return false;
    }
    return true;
}