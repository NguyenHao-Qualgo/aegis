#pragma once

#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#include "utils.h"

// Shared variables
namespace Env {
// hardcoded port and address for local discovery
constexpr int ControllerPort = 4000;
constexpr int IOT_CAMERA_API = 8500;
constexpr const char* CAMERA_IP = "127.0.0.1";
constexpr int CAMERA_PORT = 3000;
constexpr int RecoverIotHubSecs = 30;
constexpr int KeepAliveSecs = 240;

inline std::filesystem::path get_shared_mount() {
    if (CAMERA_TYPE == "L6Q")
        return "/mnt/media";
    return "/mnt/crypt_UDA";
}

const std::filesystem::path shared_mount = get_shared_mount();

const std::filesystem::path ConfigBaseDirectory = shared_mount / "IoTHubConfig";
constexpr const char* IoTHubConfigName = "iot.conf";

// ONVIF configurations
const std::filesystem::path MediaConfigBase = shared_mount / "ONVIFService/MediaConfig";
const std::filesystem::path VideoEncoderConfig = MediaConfigBase / "VideoEncoderConfig";
const std::filesystem::path VideoSourceConfig = MediaConfigBase / "VideoSourceConfig";

// Certificate
const std::filesystem::path _ssl_certificate = ConfigBaseDirectory / "camera.cer";
const std::filesystem::path _ssl_priv_key = ConfigBaseDirectory / "camera.key";

}  // namespace Env
