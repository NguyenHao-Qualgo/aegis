#include "MediaDeviceEvent.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace media {

// DataInterface implementation
nlohmann::json DataInterface::toJson() const {
    nlohmann::json j;
    j["unicastURL"] = m_unicastURL;
    j["rank"] = toString(m_rank);
    if (!m_encoding.empty())
        j["encoding"] = m_encoding;
    if (!m_framerate.empty())
        j["framerate"] = m_framerate;
    if (!m_bitrate.empty())
        j["bitrate"] = m_bitrate;
    return j;
}

bool DataInterface::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("unicastURL"))
            setUnicastURL(json["unicastURL"]);
        if (json.contains("rank"))
            setRank(rankFromString(json["rank"]));
        if (json.contains("encoding"))
            setEncoding(json["encoding"]);
        if (json.contains("framerate"))
            setFramerate(json["framerate"]);
        if (json.contains("bitrate"))
            setBitrate(json["bitrate"]);
        return true;
    } catch (...) {
        return false;
    }
}

DataInterfaceRank DataInterface::rankFromString(const std::string& str) {
    if (str == "PRIMARY")
        return DataInterfaceRank::PRIMARY;
    if (str == "SECONDARY")
        return DataInterfaceRank::SECONDARY;
    if (str == "BACKUP")
        return DataInterfaceRank::BACKUP;
    return DataInterfaceRank::PRIMARY;
}

std::string DataInterface::toString(DataInterfaceRank rank) {
    switch (rank) {
        case DataInterfaceRank::PRIMARY:
            return "PRIMARY";
        case DataInterfaceRank::SECONDARY:
            return "SECONDARY";
        case DataInterfaceRank::BACKUP:
            return "BACKUP";
        default:
            return "UNKNOWN";
    }
}

// DataSource implementation
nlohmann::json DataSource::toJson() const {
    nlohmann::json j;
    j["type"] = toString(m_type);
    j["id"] = m_id;
    j["isOnline"] = m_isOnline;
    j["capabilities"] = m_capabilities;

    nlohmann::json interfacesJson = nlohmann::json::array();
    for (const auto& interface : m_dataInterfaces) {
        interfacesJson.push_back(interface.toJson());
    }
    j["dataInterfaces"] = interfacesJson;

    return j;
}

bool DataSource::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("type"))
            setType(typeFromString(json["type"]));
        if (json.contains("id"))
            setId(json["id"]);
        if (json.contains("name"))
            setName(json["name"]);
        if (json.contains("isOnline"))
            setIsOnline(json["isOnline"]);
        if (json.contains("isStreaming"))
            setIsStreaming(json["isStreaming"]);
        if (json.contains("isStreamingCapable"))
            setIsStreamingCapable(json["isStreamingCapable"]);
        if (json.contains("isRecording"))
            setIsRecording(json["isRecording"]);

        if (json.contains("capabilities") && json["capabilities"].is_array()) {
            std::vector<std::string> capabilities;
            for (const auto& cap : json["capabilities"]) {
                capabilities.push_back(cap);
            }
            setCapabilities(capabilities);
        }

        if (json.contains("dataInterfaces") && json["dataInterfaces"].is_array()) {
            m_dataInterfaces.clear();
            for (const auto& intfJson : json["dataInterfaces"]) {
                DataInterface interface;
                if (interface.fromJson(intfJson)) {
                    addDataInterface(interface);
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

DataSourceType DataSource::typeFromString(const std::string& str) {
    if (str == "VIDEO")
        return DataSourceType::VIDEO;
    if (str == "AUDIO")
        return DataSourceType::AUDIO;
    if (str == "METADATA")
        return DataSourceType::METADATA;
    return DataSourceType::VIDEO;
}

std::string DataSource::toString(DataSourceType type) {
    switch (type) {
        case DataSourceType::VIDEO:
            return "VIDEO";
        case DataSourceType::AUDIO:
            return "AUDIO";
        case DataSourceType::METADATA:
            return "METADATA";
        default:
            return "UNKNOWN";
    }
}

// MediaDeviceEvent implementation
MediaDeviceEvent::MediaDeviceEvent(DeviceEventType type) : m_eventType(type) {
    auto now = std::chrono::system_clock::now();
    m_createdTimestamp = now;
    m_updatedTimestamp = now;
}

MediaDeviceEvent::MediaDeviceEvent(const std::string& customerId, const std::string& applianceId,
    const std::string& deviceId, const std::string& deviceLabel, const std::string& ipAddress,
    const std::string& deviceStatus, DeviceEventType type)
    : m_customerId(customerId),
      m_applianceId(applianceId),
      m_id(deviceId),
      m_label(deviceLabel),
      m_ipAddress(ipAddress),
      m_eventType(type) {
    auto now = std::chrono::system_clock::now();
    m_createdTimestamp = now;
    m_updatedTimestamp = now;
    setOnlineStatus(deviceStatus == "ONLINE");
}

nlohmann::json MediaDeviceEvent::toJson() const {
    nlohmann::json result;

    result["customerId"] = m_customerId;
    result["applianceId"] = m_applianceId;
    result["mediaDeviceType"] = toString(m_deviceType);
    result["deploymentType"] = toString(m_deploymentType);
    result["applianceType"] = toString(m_applianceType);
    result["id"] = m_id;
    result["label"] = m_label;
    result["ipAddress"] = m_ipAddress;
    result["modelNumber"] = m_modelNumber;
    result["manufacturer"] = m_manufacturer;
    result["serialNumber"] = m_serialNumber;
    result["macAddress"] = m_macAddress;
    result["firmwareVersion"] = m_firmwareVersion;
    result["dataSources"] = nlohmann::json::array();

    for (const auto& source : m_dataSources) {
        result["dataSources"].push_back(source.toJson());
    }

    result["presets"] = m_presets;
    result["status"] = toString(m_status);
    result["isOnline"] = m_isOnline;
    result["analyticEventInfo"] = m_analyticEventInfo;
    result["snapshotEventInfo"] = m_snapshotEventInfo;
    result["cameraCapabilities"] = m_cameraCapabilities;
    result["ownership"] = toString(m_ownership);
    result["createdTimestamp"] = formatTimestamp(m_createdTimestamp);

    return result;
}

bool MediaDeviceEvent::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("customerId"))
            m_customerId = json["customerId"];
        if (json.contains("applianceId"))
            m_applianceId = json["applianceId"];
        if (json.contains("id"))
            m_id = json["id"];
        if (json.contains("label"))
            m_label = json["label"];
        if (json.contains("ipAddress"))
            m_ipAddress = json["ipAddress"];
        if (json.contains("manufacturer"))
            m_manufacturer = json["manufacturer"];
        if (json.contains("modelNumber"))
            m_modelNumber = json["modelNumber"];
        if (json.contains("serialNumber"))
            m_serialNumber = json["serialNumber"];
        if (json.contains("macAddress"))
            m_macAddress = json["macAddress"];
        if (json.contains("firmwareVersion"))
            m_firmwareVersion = json["firmwareVersion"];

        if (json.contains("mediaDeviceType"))
            m_deviceType = deviceTypeFromString(json["mediaDeviceType"]);
        if (json.contains("deploymentType"))
            m_deploymentType = deploymentTypeFromString(json["deploymentType"]);
        if (json.contains("applianceType"))
            m_applianceType = applianceTypeFromString(json["applianceType"]);
        if (json.contains("status"))
            m_status = deviceStatusFromString(json["status"]);
        if (json.contains("isOnline"))
            m_isOnline = json["isOnline"];
        if (json.contains("ownership"))
            m_ownership = ownershipFromString(json["ownership"]);

        if (json.contains("cameraCapabilities") && json["cameraCapabilities"].is_array()) {
            m_cameraCapabilities.clear();
            for (const auto& cap : json["cameraCapabilities"]) {
                m_cameraCapabilities.push_back(cap);
            }
        }

        // Parse dataSources
        if (json.contains("dataSources") && json["dataSources"].is_array()) {
            m_dataSources.clear();
            for (const auto& sourceJson : json["dataSources"]) {
                DataSource source;
                if (source.fromJson(sourceJson)) {
                    m_dataSources.push_back(source);
                }
            }
        }

        // Parse event-specific fields
        if (json.contains("presets") && json["presets"].is_array()) {
            m_presets = json["presets"].get<std::vector<nlohmann::json>>();
        }

        if (json.contains("analyticEventInfo") && json["analyticEventInfo"].is_array()) {
            m_analyticEventInfo = json["analyticEventInfo"].get<std::vector<nlohmann::json>>();
        }

        if (json.contains("snapshotEventInfo") && json["snapshotEventInfo"].is_array()) {
            m_snapshotEventInfo = json["snapshotEventInfo"].get<std::vector<nlohmann::json>>();
        }

        return true;
    } catch (...) {
        return false;
    }
}

void MediaDeviceEvent::updateTimestamp() {
    m_updatedTimestamp = std::chrono::system_clock::now();
}

void MediaDeviceEvent::setOnlineStatus(bool online) {
    m_isOnline = online;
    m_status = online ? DeviceStatus::ONLINE : DeviceStatus::OFFLINE;
    updateTimestamp();

    for (auto& source : m_dataSources) {
        source.setIsOnline(online);
        source.setIsStreaming(online && source.getIsStreamingCapable());
    }
}

void MediaDeviceEvent::addDataSource(const DataSource& source) {
    m_dataSources.push_back(source);
}

void MediaDeviceEvent::addCapability(const std::string& capability) {
    if (std::find(m_cameraCapabilities.begin(), m_cameraCapabilities.end(), capability) == m_cameraCapabilities.end()) {
        m_cameraCapabilities.push_back(capability);
    }
}

bool MediaDeviceEvent::hasCapability(const std::string& capability) const {
    return std::find(m_cameraCapabilities.begin(), m_cameraCapabilities.end(), capability) !=
           m_cameraCapabilities.end();
}

void MediaDeviceEvent::setEventType(DeviceEventType type) {
    m_eventType = type;
}

// Static methods
std::string MediaDeviceEvent::toString(DeviceType type) {
    switch (type) {
        case DeviceType::FIXEDMOUNTVIDEO:
            return "FIXEDMOUNTVIDEO";
        case DeviceType::MOBILEVIDEO:
            return "MOBILEVIDEO";
        case DeviceType::AUDIO:
            return "AUDIO";
        case DeviceType::LPR:
            return "LPR";
        default:
            return "UNKNOWN";
    }
}

std::string MediaDeviceEvent::toString(DeploymentType type) {
    switch (type) {
        case DeploymentType::PUBLICSAFETY:
            return "PUBLICSAFETY";
        case DeploymentType::COMMERCIAL:
            return "COMMERCIAL";
        case DeploymentType::RESIDENTIAL:
            return "RESIDENTIAL";
        default:
            return "UNKNOWN";
    }
}

std::string MediaDeviceEvent::toString(ApplianceType type) {
    switch (type) {
        case ApplianceType::VIDEO:
            return "VIDEO";
        case ApplianceType::AUDIO:
            return "AUDIO";
        case ApplianceType::ANALYTICS:
            return "ANALYTICS";
        default:
            return "UNKNOWN";
    }
}

std::string MediaDeviceEvent::toString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::ONLINE:
            return "ONLINE";
        case DeviceStatus::OFFLINE:
            return "OFFLINE";
        case DeviceStatus::MAINTENANCE:
            return "MAINTENANCE";
        case DeviceStatus::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string MediaDeviceEvent::toString(Ownership ownership) {
    switch (ownership) {
        case Ownership::UNKNOWN:
            return "UNKNOWN";
        case Ownership::PUBLIC:
            return "PUBLIC";
        case Ownership::PRIVATE:
            return "PRIVATE";
        case Ownership::SHARED:
            return "SHARED";
        default:
            return "UNKNOWN";
    }
}

std::string MediaDeviceEvent::toString(DeviceEventType type) {
    switch (type) {
        case DeviceEventType::PRESENCE:
            return "PRESENCE";
        case DeviceEventType::DISCOVERY:
            return "DISCOVERY";
        case DeviceEventType::UPDATE:
            return "UPDATE";
        case DeviceEventType::DELETE:
            return "DELETE";
        case DeviceEventType::STREAMING_STATE:
            return "STREAMING_STATE";
        case DeviceEventType::FAULT:
            return "FAULT";
        default:
            return "UNKNOWN";
    }
}

std::string MediaDeviceEvent::formatTimestamp(const std::chrono::system_clock::time_point& timePoint) {
    auto timeT = std::chrono::system_clock::to_time_t(timePoint);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&timeT), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

DeviceType MediaDeviceEvent::deviceTypeFromString(const std::string& str) {
    if (str == "FIXEDMOUNTVIDEO")
        return DeviceType::FIXEDMOUNTVIDEO;
    if (str == "MOBILEVIDEO")
        return DeviceType::MOBILEVIDEO;
    if (str == "AUDIO")
        return DeviceType::AUDIO;
    if (str == "LPR")
        return DeviceType::LPR;
    return DeviceType::FIXEDMOUNTVIDEO;
}

DeploymentType MediaDeviceEvent::deploymentTypeFromString(const std::string& str) {
    if (str == "PUBLICSAFETY")
        return DeploymentType::PUBLICSAFETY;
    if (str == "COMMERCIAL")
        return DeploymentType::COMMERCIAL;
    if (str == "RESIDENTIAL")
        return DeploymentType::RESIDENTIAL;
    return DeploymentType::PUBLICSAFETY;
}

ApplianceType MediaDeviceEvent::applianceTypeFromString(const std::string& str) {
    if (str == "VIDEO")
        return ApplianceType::VIDEO;
    if (str == "AUDIO")
        return ApplianceType::AUDIO;
    if (str == "ANALYTICS")
        return ApplianceType::ANALYTICS;
    return ApplianceType::VIDEO;
}

DeviceStatus MediaDeviceEvent::deviceStatusFromString(const std::string& str) {
    if (str == "ONLINE")
        return DeviceStatus::ONLINE;
    if (str == "OFFLINE")
        return DeviceStatus::OFFLINE;
    if (str == "MAINTENANCE")
        return DeviceStatus::MAINTENANCE;
    if (str == "ERROR")
        return DeviceStatus::ERROR;
    return DeviceStatus::OFFLINE;
}

Ownership MediaDeviceEvent::ownershipFromString(const std::string& str) {
    if (str == "UNKNOWN")
        return Ownership::UNKNOWN;
    if (str == "PUBLIC")
        return Ownership::PUBLIC;
    if (str == "PRIVATE")
        return Ownership::PRIVATE;
    if (str == "SHARED")
        return Ownership::SHARED;
    return Ownership::UNKNOWN;
}

DeviceEventType MediaDeviceEvent::eventTypeFromString(const std::string& str) {
    if (str == "PRESENCE")
        return DeviceEventType::PRESENCE;
    if (str == "DISCOVERY")
        return DeviceEventType::DISCOVERY;
    if (str == "UPDATE")
        return DeviceEventType::UPDATE;
    if (str == "DELETE")
        return DeviceEventType::DELETE;
    if (str == "STREAMING_STATE")
        return DeviceEventType::STREAMING_STATE;
    if (str == "FAULT")
        return DeviceEventType::FAULT;
    return DeviceEventType::PRESENCE;
}

}  // namespace media