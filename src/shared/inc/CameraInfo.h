#pragma once

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "EventInterface.h"

struct DeviceInformation {
    std::string manufacturer;
    std::string model;
    std::string firmwareVersion;
    std::string serialNumber;
    std::string hardwareId;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct VideoEncoderConfiguration {
    std::string configToken;
    std::string configName;
    bool fixedConfig = false;
    int useCount = 0;

    // VideoResolution
    int width = 0;
    int height = 0;

    std::string encoder;
    bool guaranteedFramerate = false;
    int videoQuality = 0;
    int sessionTimeout = 0;

    // VideoRateControl
    int bitrateLimit = 0;
    int encodingInterval = 0;
    int frameRateLimit = 0;

    // H264EncodingConfig
    int govLength = 0;
    std::string h264Profile;

    int minimumBitrate = 0;
    int minimumFramerate = 0;
    int minimumQuality = 0;
    int maximumQuality = 0;
    int versionNumber = 0;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct VideoSourceConfiguration {
    std::string configToken;
    std::string sourceToken;
    std::string configName;
    int useCount = 0;
    bool fixedConfig = false;

    // RectangleBoundConfig
    int xBound = 0;
    int yBound = 0;
    int width = 0;
    int height = 0;

    std::string streamUri;
    std::string streamChannel;

    // StreamPorts
    int rtspPort = 0;
    int rtspsPort = 0;
    int httpPort = 0;

    int versionNumber = 0;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct PTZConfiguration {
    std::string token;
    std::string name;
    int useCount = 0;
    std::string nodeToken;
    std::string defaultRelativeZoomTranslationSpace;
    std::string defaultContinuousZoomVelocitySpace;
    std::string defaultPTZTimeout;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct MetadataConfiguration {
    std::string token;
    std::string name;
    int useCount = 0;
    bool analytics = false;
    std::string sessionTimeout;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct Profile {
    std::string token;
    bool fixed = false;
    std::string name;
    VideoSourceConfiguration videoSourceConfig;
    VideoEncoderConfiguration videoEncoderConfig;
    PTZConfiguration ptzConfig;
    MetadataConfiguration metadataConfig;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct ProfilesResponse {
    std::vector<Profile> profiles;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

struct MediaUri {
    Profile profile;
    std::string uri;
    bool invalidAfterConnect = false;
    bool invalidAfterReboot = false;
    std::string timeout;

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
};

class CameraInfo : public EventInterface {
   private:
    // Core identification
    std::string m_id;
    std::string m_ipAddress;
    std::string m_name;
    std::string m_macAddress;

    // Device information
    DeviceInformation m_deviceInfo;

    // dataSources
    VideoSourceConfiguration m_videoSourceConfig;
    VideoEncoderConfiguration m_videoEncoderConfig;

    // Capabilities
    std::vector<std::string> m_capabilities;
    bool m_supportsVideo;
    bool m_supportsAudio;
    bool m_supportsPTZ;

    // Connection details
    int m_port;
    std::string m_username;
    std::string m_password;
    std::string m_deviceId;

    // IsSet flags
    bool m_idIsSet;
    bool m_ipAddressIsSet;
    bool m_nameIsSet;
    bool m_macAddressIsSet;
    bool m_deviceInfoIsSet;
    bool m_supportsVideoIsSet;
    bool m_supportsAudioIsSet;
    bool m_supportsPTZIsSet;
    bool m_portIsSet;
    bool m_usernameIsSet;
    bool m_passwordIsSet;
    bool m_deviceIdIsSet;
    bool m_videoSourceConfigIsSet;
    bool m_videoEncoderConfigIsSet;

   public:
    CameraInfo();

    // Setters
    void setId(const std::string& id);
    void setIpAddress(const std::string& ipAddress);
    void setName(const std::string& name);
    void setMacAddress(const std::string& macAddress);
    void setDeviceInfo(const DeviceInformation& deviceInfo);
    void setSupportsVideo(bool supports);
    void setSupportsAudio(bool supports);
    void setSupportsPTZ(bool supports);
    void setPort(int port);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setDeviceId(const std::string& deviceId);
    void addCapability(const std::string& capability);
    void setVideoSourceConfig(const VideoSourceConfiguration& videoSourceConfig);
    void setVideoEncoderConfig(const VideoEncoderConfiguration& videoEncoderConfig);

    // Getters
    std::string getId() const {
        return m_id;
    }
    std::string getIpAddress() const {
        return m_ipAddress;
    }
    std::string getName() const {
        return m_name;
    }
    std::string getMacAddress() const {
        return m_macAddress;
    }
    const DeviceInformation& getDeviceInfo() const {
        return m_deviceInfo;
    }
    const std::vector<std::string>& getCapabilities() const {
        return m_capabilities;
    }
    bool getSupportsVideo() const {
        return m_supportsVideo;
    }
    bool getSupportsAudio() const {
        return m_supportsAudio;
    }
    bool getSupportsPTZ() const {
        return m_supportsPTZ;
    }
    int getPort() const {
        return m_port;
    }
    std::string getUsername() const {
        return m_username;
    }
    std::string getPassword() const {
        return m_password;
    }
    std::string getDeviceId() const {
        return m_deviceId;
    }

    VideoEncoderConfiguration getVideoEncoderConfig() const {
        return m_videoEncoderConfig;
    }

    VideoSourceConfiguration getVideoSourceConfig() const {
        return m_videoSourceConfig;
    }

    // Utility methods
    bool isValid() const;
    bool hasCapability(const std::string& capability) const;

    // EventInterface implementation
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
};