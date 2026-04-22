#pragma once

#include <functional>

#include "IoTHubInterface.h"
#include "common.h"

class IoTHubDirectMethod {
    IoTHubInterface& iot_hub_if_;
    std::function<CommandResponse(const std::string&, const std::string&)> method_callback_;
    static void methodCallback(const IOTHUB_CLIENT_COMMAND_REQUEST* command_request,
        IOTHUB_CLIENT_COMMAND_RESPONSE* command_response, void* user_context_callback);

   public:
    explicit IoTHubDirectMethod(IoTHubInterface& iot_hub_interface);
    void OnMethodReceived(std::function<CommandResponse(const std::string&, const std::string&)>& method_cb);
    bool Register(IOTHUB_CLIENT_CORE_HANDLE& client_handle);

   private:
};