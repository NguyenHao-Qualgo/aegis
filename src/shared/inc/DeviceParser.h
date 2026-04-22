#pragma once

#include <string>
#include <vector>

#include "CameraInfo.h"
#include "tinyxml2.h"

class DeviceParser {
   public:
    static DeviceInformation parseDeviceInformation(const std::string& xml);
    static VideoSourceConfiguration parseVideoSourceConfig(const std::string& xml);
    static VideoEncoderConfiguration parseVideoEncoderConfig(const std::string& xml);
};
