#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "CameraInfo.h"

struct DiscoveryResult {
    CameraInfo camera;
    std::chrono::steady_clock::time_point discoveryTime;
    std::chrono::milliseconds discoveryDuration{0};
    bool successful = false;
    std::string error;

    static DiscoveryResult success(CameraInfo camera, std::chrono::milliseconds duration) {
        DiscoveryResult result;
        result.camera = std::move(camera);
        result.discoveryDuration = duration;
        result.successful = true;
        result.discoveryTime = std::chrono::steady_clock::now();
        return result;
    }

    static DiscoveryResult failure(const std::string& errorMessage) {
        DiscoveryResult result;
        result.error = errorMessage;
        result.successful = false;
        result.discoveryTime = std::chrono::steady_clock::now();
        return result;
    }
};
