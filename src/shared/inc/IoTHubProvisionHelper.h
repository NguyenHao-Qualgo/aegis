#pragma once

#include <azure_prov_client/prov_device_client.h>

#include <string>

class IoTProvisionHelper {
   public:
    static IoTProvisionHelper& Instance() {
        static IoTProvisionHelper instance;
        return instance;
    }
    PROV_DEVICE_RESULT Provision(const std::string& cert_path, const std::string& key_path,
        const std::string& prov_host, const std::string& id_scope);
    std::string GetIoTHubUri() const {
        return iothub_uri_;
    }
    std::string GetDeviceId() const {
        return device_id_;
    }

   private:
    IoTProvisionHelper() = default;
    IoTProvisionHelper(const IoTProvisionHelper&) = delete;
    IoTProvisionHelper& operator=(const IoTProvisionHelper&) = delete;

    std::string iothub_uri_;
    std::string device_id_;
};