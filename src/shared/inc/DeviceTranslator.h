#pragma once
#include <string>
#include <vector>

#include "CameraInfo.h"
#include "MediaDeviceEvent.h"

namespace media {

class DeviceTranslator {
   public:
    static UnifiedMediaDeviceEvent CreateUnifiedMediaDeviceEvent(const CameraInfo& camera,
        const std::string& customerId, const std::string& applianceId,
        ApplianceType applianceType = ApplianceType::VIDEO, DeviceStatus status = DeviceStatus::ONLINE) {
        UnifiedMediaDeviceEvent device;

        // Set core identification
        device.fromJson(camera.getDeviceInfo().toJson());
        device.setCustomerId(customerId);
        device.setApplianceId(applianceId);
        device.setLabel(camera.getName());
        device.setId(camera.getId());
        device.setIpAddress(camera.getIpAddress());

        // TODO: remove hardcoded values
        device.setDeviceType(DeviceType::FIXEDMOUNTVIDEO);
        device.setDeploymentType(DeploymentType::PUBLICSAFETY);
        device.setApplianceType(applianceType);
        device.setOnlineStatus(status == DeviceStatus::ONLINE);
        device.addCapability("PTZ_PATTERNS");
        device.addCapability("PTZ_PRESETS");
        device.addCapability("ZOOM_VELOCITY");

        auto videoSourceConfig = camera.getVideoSourceConfig();

        DataSource videoSource;
        videoSource.setType(DataSourceType::VIDEO);
        videoSource.setId(camera.getId() + ":" + videoSourceConfig.sourceToken);
        videoSource.setIsOnline(device.getIsOnline());
        videoSource.setIsStreamingCapable(true);
        videoSource.setIsStreaming(device.getIsOnline());
        videoSource.setIsRecording(false);
        // videoSource.setCapabilities({"SNAPSHOT"});
        const std::string URL = "rtsp://" + videoSourceConfig.streamUri + ":" +
                                std::to_string(videoSourceConfig.rtspPort) + "/" + videoSourceConfig.streamChannel;
        DataInterface interface;
        interface.setUnicastURL(URL);
        interface.setRank(DataInterfaceRank::PRIMARY);
        videoSource.addDataInterface(interface);

        device.addDataSource(videoSource);

        if (camera.getSupportsPTZ()) {
            device.addCapability("PTZ");
            device.addCapability("PTZ_PRESETS");
            device.addCapability("PTZ_PATTERNS");
        }

        return device;
    }
};

}  // namespace media

// Backward compatibility alias
using DeviceTranslator = media::DeviceTranslator;
