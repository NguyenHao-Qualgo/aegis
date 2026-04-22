#include "IoTHubProvisionHelper.h"

#include <azure_c_shared_utility/http_proxy_io.h>
#include <azure_c_shared_utility/shared_util_options.h>
#include <azure_c_shared_utility/threadapi.h>
#include <azure_c_shared_utility/tickcounter.h>
#include <azure_prov_client/prov_device_client.h>
#include <azure_prov_client/prov_security_factory.h>
#include <azure_prov_client/prov_transport_mqtt_client.h>
#include <iothub.h>
#include <iothub_client_options.h>
#include <iothub_client_version.h>
#include <iothub_device_client_ll.h>
#include <iothub_message.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "env.h"
#include "logging.h"

static std::string read_pem_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        LOG_E("Failed to open PEM file: {}", filename);
        return {};
    }
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return std::string(buffer.begin(), buffer.end());
}

struct ProvisionContext {
    bool complete = false;
    PROV_DEVICE_RESULT register_result = PROV_DEVICE_RESULT_ERROR;
};

PROV_DEVICE_RESULT IoTProvisionHelper::Provision(const std::string& cert_path, const std::string& key_path,
        const std::string& prov_host, const std::string& id_scope) {
    PROV_DEVICE_RESULT result = PROV_DEVICE_RESULT_ERROR;
    std::string x509certificate = read_pem_file(cert_path);
    std::string x509privatekey = read_pem_file(key_path);
    if (x509certificate.empty() || x509privatekey.empty()) {
        LOG_E("Failed to load certificate or private key from file: {}, {}", cert_path, key_path);
        return result;
    }

    if (prov_host.empty() || id_scope.empty()) {
        LOG_E("No Prov Host or Id Scope to DPS");
        return PROV_DEVICE_RESULT_DEV_AUTH_ERROR;
    }

    const std::string registration_id = ReadCertificateCN(cert_path);
    if (registration_id.empty()) {
        LOG_E("Failed to extract registration ID (CN) from certificate: {}", cert_path);
        return PROV_DEVICE_RESULT_PARSING;
    }

    if (IoTHub_Init() != 0) {
        LOG_E("IoTHub_Init failed");
        return PROV_DEVICE_RESULT_ERROR;
    }
    if (prov_dev_security_init(SECURE_DEVICE_TYPE_X509) != 0) {
        LOG_E("prov_dev_security_init failed");
        IoTHub_Deinit();
        return PROV_DEVICE_RESULT_KEY_ERROR;
    }

    PROV_DEVICE_HANDLE handle = Prov_Device_Create(prov_host.c_str(), id_scope.c_str(), Prov_Device_MQTT_Protocol);
    if (!handle) {
        LOG_E("failed calling Prov_Device_Create");
        prov_dev_security_deinit();
        IoTHub_Deinit();
        return PROV_DEVICE_RESULT_ERROR;
    }

    ProvisionContext context;
    std::string iothub_uri, device_id;

    auto register_device_callback = [](PROV_DEVICE_RESULT register_result, const char* iothub_uri_c,
                                        const char* device_id_c, void* user_context) {
        auto* ctx = static_cast<ProvisionContext*>(user_context);
        ctx->complete = true;
        ctx->register_result = register_result;
        if (register_result == PROV_DEVICE_RESULT_OK && iothub_uri_c && device_id_c) {
            IoTProvisionHelper::Instance().iothub_uri_ = iothub_uri_c;
            IoTProvisionHelper::Instance().device_id_ = device_id_c;
        } else {
            LOG_E("Provisioning failed: result={}, iothub_uri={}, device_id={}", static_cast<int>(register_result),
                iothub_uri_c ? iothub_uri_c : "(null)", device_id_c ? device_id_c : "(null)");
        }
    };

    if (Prov_Device_SetOption(handle, OPTION_X509_CERT, x509certificate.c_str()) != PROV_DEVICE_RESULT_OK ||
        Prov_Device_SetOption(handle, OPTION_X509_PRIVATE_KEY, x509privatekey.c_str()) != PROV_DEVICE_RESULT_OK ||
        Prov_Device_SetOption(handle, PROV_REGISTRATION_ID, registration_id.c_str()) != PROV_DEVICE_RESULT_OK) {
        LOG_E("Failed to set provisioning options");
        Prov_Device_Destroy(handle);
        prov_dev_security_deinit();
        IoTHub_Deinit();
        return PROV_DEVICE_RESULT_ERROR;
    }

    if (Prov_Device_Register_Device(handle, register_device_callback, &context, nullptr, nullptr) !=
        PROV_DEVICE_RESULT_OK) {
        LOG_E("Prov_Device_Register_Device failed");
        Prov_Device_Destroy(handle);
        prov_dev_security_deinit();
        IoTHub_Deinit();
        return PROV_DEVICE_RESULT_ERROR;
    }

    const int max_wait_ms = 60000;
    int waited_ms = 0;
    const int poll_interval_ms = 100;
    while (!context.complete && waited_ms < max_wait_ms) {
        ThreadAPI_Sleep(poll_interval_ms);
        waited_ms += poll_interval_ms;
    }

    if (!context.complete) {
        LOG_E("Provisioning timed out after {} ms", max_wait_ms);
        Prov_Device_Destroy(handle);
        prov_dev_security_deinit();
        IoTHub_Deinit();
        return PROV_DEVICE_RESULT_TIMEOUT;
    }

    Prov_Device_Destroy(handle);
    prov_dev_security_deinit();
    IoTHub_Deinit();

    if (iothub_uri_.empty() || device_id_.empty()) {
        LOG_W("Provisioning did not return IoT Hub URI or device ID");
    }

    LOG_D("Provisioning succeeded: IoT Hub URI={}, Device ID={}, res {}", iothub_uri_, device_id_,
        static_cast<int>(context.register_result));
    return context.register_result;
}