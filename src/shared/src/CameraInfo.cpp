#include "CameraInfo.h"

#include <algorithm>

// DeviceInformation implementation
nlohmann::json DeviceInformation::toJson() const {
    nlohmann::json j;
    if (!manufacturer.empty())
        j["manufacturer"] = manufacturer;
    if (!model.empty())
        j["model"] = model;
    if (!firmwareVersion.empty())
        j["firmwareVersion"] = firmwareVersion;
    if (!serialNumber.empty())
        j["serialNumber"] = serialNumber;
    if (!hardwareId.empty())
        j["hardwareId"] = hardwareId;
    return j;
}

bool DeviceInformation::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("manufacturer"))
            manufacturer = json["manufacturer"];
        if (json.contains("model"))
            model = json["model"];
        if (json.contains("firmwareVersion"))
            firmwareVersion = json["firmwareVersion"];
        if (json.contains("serialNumber"))
            serialNumber = json["serialNumber"];
        if (json.contains("hardwareId"))
            hardwareId = json["hardwareId"];
        return true;
    } catch (...) {
        return false;
    }
}

// VideoSourceConfiguration implementation
nlohmann::json VideoSourceConfiguration::toJson() const {
    nlohmann::json j;
    if (!configToken.empty())
        j["configToken"] = configToken;
    if (!sourceToken.empty())
        j["sourceToken"] = sourceToken;
    if (!configName.empty())
        j["configName"] = configName;
    j["useCount"] = useCount;
    j["fixedConfig"] = fixedConfig;
    j["rectangleBoundConfig"] = {{"xBound", xBound}, {"yBound", yBound}, {"width", width}, {"height", height}};
    if (!streamUri.empty())
        j["streamUri"] = streamUri;
    if (!streamChannel.empty())
        j["streamChannel"] = streamChannel;
    j["streamPorts"] = {{"rtspPort", rtspPort}, {"rtspsPort", rtspsPort}, {"httpPort", httpPort}};
    j["versionNumber"] = versionNumber;
    return j;
}

bool VideoSourceConfiguration::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("configToken"))
            configToken = json["configToken"];
        if (json.contains("sourceToken"))
            sourceToken = json["sourceToken"];
        if (json.contains("configName"))
            configName = json["configName"];
        if (json.contains("useCount"))
            useCount = json["useCount"];
        if (json.contains("fixedConfig"))
            fixedConfig = json["fixedConfig"];
        if (json.contains("rectangleBoundConfig")) {
            auto& rect = json["rectangleBoundConfig"];
            if (rect.contains("xBound"))
                xBound = rect["xBound"];
            if (rect.contains("yBound"))
                yBound = rect["yBound"];
            if (rect.contains("width"))
                width = rect["width"];
            if (rect.contains("height"))
                height = rect["height"];
        }
        if (json.contains("streamUri"))
            streamUri = json["streamUri"];
        if (json.contains("streamChannel"))
            streamChannel = json["streamChannel"];
        if (json.contains("streamPorts")) {
            auto& ports = json["streamPorts"];
            if (ports.contains("rtspPort"))
                rtspPort = ports["rtspPort"];
            if (ports.contains("rtspsPort"))
                rtspsPort = ports["rtspsPort"];
            if (ports.contains("httpPort"))
                httpPort = ports["httpPort"];
        }
        if (json.contains("versionNumber"))
            versionNumber = json["versionNumber"];
        return true;
    } catch (...) {
        return false;
    }
}

// VideoEncoderConfiguration implementation
nlohmann::json VideoEncoderConfiguration::toJson() const {
    nlohmann::json j;
    if (!configToken.empty())
        j["configToken"] = configToken;
    if (!configName.empty())
        j["configName"] = configName;
    j["fixedConfig"] = fixedConfig;
    j["useCount"] = useCount;
    j["videoResolution"] = {{"width", width}, {"height", height}};
    if (!encoder.empty())
        j["encoder"] = encoder;
    j["guaranteedFramerate"] = guaranteedFramerate;
    j["videoQuality"] = videoQuality;
    j["sessionTimeout"] = sessionTimeout;
    j["videoRateControl"] = {
        {"bitrateLimit", bitrateLimit}, {"encodingInterval", encodingInterval}, {"frameRateLimit", frameRateLimit}};
    j["h264EncodingConfig"] = {{"govLength", govLength}, {"h264Profile", h264Profile}};
    j["minimumBitrate"] = minimumBitrate;
    j["minimumFramerate"] = minimumFramerate;
    j["minimumQuality"] = minimumQuality;
    j["maximumQuality"] = maximumQuality;
    j["versionNumber"] = versionNumber;
    return j;
}

bool VideoEncoderConfiguration::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("configToken"))
            configToken = json["configToken"];
        if (json.contains("configName"))
            configName = json["configName"];
        if (json.contains("fixedConfig"))
            fixedConfig = json["fixedConfig"];
        if (json.contains("useCount"))
            useCount = json["useCount"];
        if (json.contains("videoResolution")) {
            auto& res = json["videoResolution"];
            if (res.contains("width"))
                width = res["width"];
            if (res.contains("height"))
                height = res["height"];
        }
        if (json.contains("encoder"))
            encoder = json["encoder"];
        if (json.contains("guaranteedFramerate"))
            guaranteedFramerate = json["guaranteedFramerate"];
        if (json.contains("videoQuality"))
            videoQuality = json["videoQuality"];
        if (json.contains("sessionTimeout"))
            sessionTimeout = json["sessionTimeout"];
        if (json.contains("videoRateControl")) {
            auto& rc = json["videoRateControl"];
            if (rc.contains("bitrateLimit"))
                bitrateLimit = rc["bitrateLimit"];
            if (rc.contains("encodingInterval"))
                encodingInterval = rc["encodingInterval"];
            if (rc.contains("frameRateLimit"))
                frameRateLimit = rc["frameRateLimit"];
        }
        if (json.contains("h264EncodingConfig")) {
            auto& h264 = json["h264EncodingConfig"];
            if (h264.contains("govLength"))
                govLength = h264["govLength"];
            if (h264.contains("h264Profile"))
                h264Profile = h264["h264Profile"];
        }
        if (json.contains("minimumBitrate"))
            minimumBitrate = json["minimumBitrate"];
        if (json.contains("minimumFramerate"))
            minimumFramerate = json["minimumFramerate"];
        if (json.contains("minimumQuality"))
            minimumQuality = json["minimumQuality"];
        if (json.contains("maximumQuality"))
            maximumQuality = json["maximumQuality"];
        if (json.contains("versionNumber"))
            versionNumber = json["versionNumber"];
        return true;
    } catch (...) {
        return false;
    }
}

// PTZConfiguration implementation
nlohmann::json PTZConfiguration::toJson() const {
    nlohmann::json j;
    if (!token.empty())
        j["token"] = token;
    if (!name.empty())
        j["name"] = name;
    j["useCount"] = useCount;
    if (!nodeToken.empty())
        j["nodeToken"] = nodeToken;
    if (!defaultRelativeZoomTranslationSpace.empty())
        j["defaultRelativeZoomTranslationSpace"] = defaultRelativeZoomTranslationSpace;
    if (!defaultContinuousZoomVelocitySpace.empty())
        j["defaultContinuousZoomVelocitySpace"] = defaultContinuousZoomVelocitySpace;
    if (!defaultPTZTimeout.empty())
        j["defaultPTZTimeout"] = defaultPTZTimeout;
    return j;
}

bool PTZConfiguration::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("token"))
            token = json["token"];
        if (json.contains("name"))
            name = json["name"];
        if (json.contains("useCount"))
            useCount = json["useCount"];
        if (json.contains("nodeToken"))
            nodeToken = json["nodeToken"];
        if (json.contains("defaultRelativeZoomTranslationSpace"))
            defaultRelativeZoomTranslationSpace = json["defaultRelativeZoomTranslationSpace"];
        if (json.contains("defaultContinuousZoomVelocitySpace"))
            defaultContinuousZoomVelocitySpace = json["defaultContinuousZoomVelocitySpace"];
        if (json.contains("defaultPTZTimeout"))
            defaultPTZTimeout = json["defaultPTZTimeout"];
        return true;
    } catch (...) {
        return false;
    }
}

// MetadataConfiguration implementation
nlohmann::json MetadataConfiguration::toJson() const {
    nlohmann::json j;
    if (!token.empty())
        j["token"] = token;
    if (!name.empty())
        j["name"] = name;
    j["useCount"] = useCount;
    j["analytics"] = analytics;
    if (!sessionTimeout.empty())
        j["sessionTimeout"] = sessionTimeout;
    return j;
}

bool MetadataConfiguration::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("token"))
            token = json["token"];
        if (json.contains("name"))
            name = json["name"];
        if (json.contains("useCount"))
            useCount = json["useCount"];
        if (json.contains("analytics"))
            analytics = json["analytics"];
        if (json.contains("sessionTimeout"))
            sessionTimeout = json["sessionTimeout"];
        return true;
    } catch (...) {
        return false;
    }
}

// Profile implementation
nlohmann::json Profile::toJson() const {
    nlohmann::json j;
    if (!token.empty())
        j["token"] = token;
    j["fixed"] = fixed;
    if (!name.empty())
        j["name"] = name;
    j["videoSourceConfiguration"] = videoSourceConfig.toJson();
    j["videoEncoderConfiguration"] = videoEncoderConfig.toJson();
    j["ptzConfiguration"] = ptzConfig.toJson();
    j["metadataConfiguration"] = metadataConfig.toJson();
    return j;
}

bool Profile::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("token"))
            token = json["token"];
        if (json.contains("fixed"))
            fixed = json["fixed"];
        if (json.contains("name"))
            name = json["name"];
        if (json.contains("videoSourceConfiguration"))
            videoSourceConfig.fromJson(json["videoSourceConfiguration"]);
        if (json.contains("videoEncoderConfiguration"))
            videoEncoderConfig.fromJson(json["videoEncoderConfiguration"]);
        if (json.contains("ptzConfiguration"))
            ptzConfig.fromJson(json["ptzConfiguration"]);
        if (json.contains("metadataConfiguration"))
            metadataConfig.fromJson(json["metadataConfiguration"]);
        return true;
    } catch (...) {
        return false;
    }
}

// ProfilesResponse implementation
nlohmann::json ProfilesResponse::toJson() const {
    nlohmann::json j;
    nlohmann::json profilesArray = nlohmann::json::array();
    for (const auto& profile : profiles) {
        profilesArray.push_back(profile.toJson());
    }
    j["profiles"] = profilesArray;
    return j;
}

bool ProfilesResponse::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("profiles") && json["profiles"].is_array()) {
            profiles.clear();
            for (const auto& profileJson : json["profiles"]) {
                Profile profile;
                if (profile.fromJson(profileJson)) {
                    profiles.push_back(profile);
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

// MediaUri implementation
nlohmann::json MediaUri::toJson() const {
    nlohmann::json j;
    j["profile"] = profile.toJson();
    if (!uri.empty())
        j["uri"] = uri;
    j["invalidAfterConnect"] = invalidAfterConnect;
    j["invalidAfterReboot"] = invalidAfterReboot;
    if (!timeout.empty())
        j["timeout"] = timeout;
    return j;
}

bool MediaUri::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("profile"))
            profile.fromJson(json["profile"]);
        if (json.contains("uri"))
            uri = json["uri"];
        if (json.contains("invalidAfterConnect"))
            invalidAfterConnect = json["invalidAfterConnect"];
        if (json.contains("invalidAfterReboot"))
            invalidAfterReboot = json["invalidAfterReboot"];
        if (json.contains("timeout"))
            timeout = json["timeout"];
        return true;
    } catch (...) {
        return false;
    }
}

// CameraInfo implementation
CameraInfo::CameraInfo()
    : m_supportsVideo(true),
      m_supportsAudio(false),
      m_supportsPTZ(false),
      m_port(8080),
      m_idIsSet(false),
      m_ipAddressIsSet(false),
      m_nameIsSet(false),
      m_macAddressIsSet(false),
      m_deviceInfoIsSet(false),
      m_supportsVideoIsSet(false),
      m_supportsAudioIsSet(false),
      m_supportsPTZIsSet(false),
      m_portIsSet(false),
      m_usernameIsSet(false),
      m_passwordIsSet(false),
      m_deviceIdIsSet(false) {
}

void CameraInfo::setId(const std::string& id) {
    m_id = id;
    m_idIsSet = true;
}

void CameraInfo::setIpAddress(const std::string& ipAddress) {
    m_ipAddress = ipAddress;
    m_ipAddressIsSet = true;
}

void CameraInfo::setName(const std::string& name) {
    m_name = name;
    m_nameIsSet = true;
}

void CameraInfo::setMacAddress(const std::string& macAddress) {
    m_macAddress = macAddress;
    m_macAddressIsSet = true;
}

void CameraInfo::setDeviceInfo(const DeviceInformation& deviceInfo) {
    m_deviceInfo = deviceInfo;
    m_deviceInfoIsSet = true;
}

void CameraInfo::setSupportsVideo(bool supports) {
    m_supportsVideo = supports;
    m_supportsVideoIsSet = true;
}

void CameraInfo::setSupportsAudio(bool supports) {
    m_supportsAudio = supports;
    m_supportsAudioIsSet = true;
}

void CameraInfo::setSupportsPTZ(bool supports) {
    m_supportsPTZ = supports;
    m_supportsPTZIsSet = true;
}

void CameraInfo::setPort(int port) {
    m_port = port;
    m_portIsSet = true;
}

void CameraInfo::setUsername(const std::string& username) {
    m_username = username;
    m_usernameIsSet = true;
}

void CameraInfo::setPassword(const std::string& password) {
    m_password = password;
    m_passwordIsSet = true;
}

void CameraInfo::setDeviceId(const std::string& deviceId) {
    m_deviceId = deviceId;
    m_deviceIdIsSet = true;
}

void CameraInfo::addCapability(const std::string& capability) {
    if (std::find(m_capabilities.begin(), m_capabilities.end(), capability) == m_capabilities.end()) {
        m_capabilities.push_back(capability);
    }
}

void CameraInfo::setVideoSourceConfig(const VideoSourceConfiguration& videoSourceConfig) {
    m_videoSourceConfig = videoSourceConfig;
    m_videoSourceConfigIsSet = true;
}
void CameraInfo::setVideoEncoderConfig(const VideoEncoderConfiguration& videoEncoderConfig) {
    m_videoEncoderConfig = videoEncoderConfig;
    m_videoEncoderConfigIsSet = true;
}

bool CameraInfo::isValid() const {
    return m_idIsSet && !m_id.empty() && m_ipAddressIsSet && !m_ipAddress.empty() && m_nameIsSet && !m_name.empty();
}

bool CameraInfo::hasCapability(const std::string& capability) const {
    return std::find(m_capabilities.begin(), m_capabilities.end(), capability) != m_capabilities.end();
}

nlohmann::json CameraInfo::toJson() const {
    nlohmann::json j;

    if (m_idIsSet)
        j["id"] = m_id;
    if (m_ipAddressIsSet)
        j["ipAddress"] = m_ipAddress;
    if (m_nameIsSet)
        j["name"] = m_name;
    if (m_macAddressIsSet)
        j["macAddress"] = m_macAddress;

    if (m_deviceInfoIsSet) {
        j["deviceInfo"] = m_deviceInfo.toJson();
    }

    if (m_videoSourceConfigIsSet) {
        j["VideoSourceConfiguration"] = m_videoSourceConfig.toJson();
    }

    if (m_videoEncoderConfigIsSet) {
        j["VideoEncoderConfiguration"] = m_videoEncoderConfig.toJson();
    }

    if (!m_capabilities.empty()) {
        j["capabilities"] = m_capabilities;
    }

    if (m_supportsVideoIsSet)
        j["supportsVideo"] = m_supportsVideo;
    if (m_supportsAudioIsSet)
        j["supportsAudio"] = m_supportsAudio;
    if (m_supportsPTZIsSet)
        j["supportsPTZ"] = m_supportsPTZ;
    if (m_portIsSet)
        j["port"] = m_port;
    if (m_usernameIsSet)
        j["username"] = m_username;
    if (m_passwordIsSet)
        j["password"] = m_password;
    if (m_deviceIdIsSet)
        j["deviceId"] = m_deviceId;

    return j;
}

bool CameraInfo::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("id"))
            setId(json["id"]);
        if (json.contains("ipAddress"))
            setIpAddress(json["ipAddress"]);
        if (json.contains("name"))
            setName(json["name"]);
        if (json.contains("macAddress"))
            setMacAddress(json["macAddress"]);

        if (json.contains("deviceInfo")) {
            DeviceInformation info;
            if (info.fromJson(json["deviceInfo"])) {
                setDeviceInfo(info);
            }
        }

        if (json.contains("videoSourceConfig")) {
            VideoSourceConfiguration vsc;
            if (vsc.fromJson(json["videoSourceConfig"])) {
                setVideoSourceConfig(vsc);
            }
        }
        if (json.contains("videoEncoderConfig")) {
            VideoEncoderConfiguration vec;
            if (vec.fromJson(json["videoEncoderConfig"])) {
                setVideoEncoderConfig(vec);
            }
        }

        if (json.contains("capabilities") && json["capabilities"].is_array()) {
            m_capabilities.clear();
            for (const auto& cap : json["capabilities"]) {
                if (cap.is_string()) {
                    addCapability(cap.get<std::string>());
                }
            }
        }

        if (json.contains("supportsVideo"))
            setSupportsVideo(json["supportsVideo"]);
        if (json.contains("supportsAudio"))
            setSupportsAudio(json["supportsAudio"]);
        if (json.contains("supportsPTZ"))
            setSupportsPTZ(json["supportsPTZ"]);
        if (json.contains("port"))
            setPort(json["port"]);
        if (json.contains("username"))
            setUsername(json["username"]);
        if (json.contains("password"))
            setPassword(json["password"]);
        if (json.contains("deviceId"))
            setDeviceId(json["deviceId"]);

        return true;
    } catch (...) {
        return false;
    }
}