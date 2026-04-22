#pragma once
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "EventInterface.h"

namespace media {

enum class DeviceType { FIXEDMOUNTVIDEO, MOBILEVIDEO, AUDIO, LPR };
enum class DeploymentType { PUBLICSAFETY, COMMERCIAL, RESIDENTIAL };
enum class ApplianceType { VIDEO, AUDIO, ANALYTICS };
enum class DataSourceType { VIDEO, AUDIO, METADATA };
enum class DataInterfaceRank { PRIMARY, SECONDARY, BACKUP };
enum class DeviceStatus { ONLINE, OFFLINE, MAINTENANCE, ERROR };
enum class Ownership { UNKNOWN, PUBLIC, PRIVATE, SHARED };
enum class DeviceEventType { PRESENCE, DISCOVERY, UPDATE, DELETE, STREAMING_STATE, FAULT };

class DataInterface {
   private:
    std::string m_unicastURL;
    DataInterfaceRank m_rank = DataInterfaceRank::PRIMARY;
    std::string m_encoding;
    std::string m_framerate;
    std::string m_bitrate;

   public:
    // Setters
    void setUnicastURL(const std::string& url) {
        m_unicastURL = url;
    }
    void setRank(DataInterfaceRank rank) {
        m_rank = rank;
    }
    void setEncoding(const std::string& encoding) {
        m_encoding = encoding;
    }
    void setFramerate(const std::string& framerate) {
        m_framerate = framerate;
    }
    void setBitrate(const std::string& bitrate) {
        m_bitrate = bitrate;
    }

    // Getters
    std::string getUnicastURL() const {
        return m_unicastURL;
    }
    DataInterfaceRank getRank() const {
        return m_rank;
    }
    std::string getEncoding() const {
        return m_encoding;
    }
    std::string getFramerate() const {
        return m_framerate;
    }
    std::string getBitrate() const {
        return m_bitrate;
    }

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
    static std::string toString(DataInterfaceRank rank);
    static DataInterfaceRank rankFromString(const std::string& str);
};

class DataSource {
   private:
    DataSourceType m_type = DataSourceType::VIDEO;
    std::string m_id;
    std::string m_name;
    bool m_isOnline = false;
    bool m_isStreaming = false;
    bool m_isStreamingCapable = true;
    bool m_isRecording = false;
    std::vector<DataInterface> m_dataInterfaces;
    std::vector<std::string> m_capabilities;

   public:
    // Setters
    void setType(DataSourceType type) {
        m_type = type;
    }
    void setId(const std::string& id) {
        m_id = id;
    }
    void setName(const std::string& name) {
        m_name = name;
    }
    void setIsOnline(bool online) {
        m_isOnline = online;
    }
    void setIsStreaming(bool streaming) {
        m_isStreaming = streaming;
    }
    void setIsStreamingCapable(bool capable) {
        m_isStreamingCapable = capable;
    }
    void setIsRecording(bool recording) {
        m_isRecording = recording;
    }
    void addDataInterface(const DataInterface& interface) {
        m_dataInterfaces.push_back(interface);
    }
    void setDataInterfaces(const std::vector<DataInterface>& interfaces) {
        m_dataInterfaces = interfaces;
    }
    void addCapability(const std::string& capability) {
        m_capabilities.push_back(capability);
    }
    void setCapabilities(const std::vector<std::string>& capabilities) {
        m_capabilities = capabilities;
    }

    // Getters
    DataSourceType getType() const {
        return m_type;
    }
    std::string getId() const {
        return m_id;
    }
    std::string getName() const {
        return m_name;
    }
    bool getIsOnline() const {
        return m_isOnline;
    }
    bool getIsStreaming() const {
        return m_isStreaming;
    }
    bool getIsStreamingCapable() const {
        return m_isStreamingCapable;
    }
    bool getIsRecording() const {
        return m_isRecording;
    }
    const std::vector<DataInterface>& getDataInterfaces() const {
        return m_dataInterfaces;
    }
    const std::vector<std::string>& getCapabilities() const {
        return m_capabilities;
    }

    nlohmann::json toJson() const;
    bool fromJson(const nlohmann::json& json);
    static std::string toString(DataSourceType type);
    static DataSourceType typeFromString(const std::string& str);
};

class MediaDeviceEvent : public EventInterface {
   private:
    // Core identification
    std::string m_customerId;
    std::string m_applianceId;
    std::string m_id;
    std::string m_label;
    std::string m_ipAddress;

    // Device information
    std::string m_manufacturer;
    std::string m_modelNumber;
    std::string m_serialNumber;
    std::string m_macAddress;
    std::string m_firmwareVersion;

    // Device types and capabilities
    DeviceType m_deviceType = DeviceType::FIXEDMOUNTVIDEO;
    DeploymentType m_deploymentType = DeploymentType::PUBLICSAFETY;
    ApplianceType m_applianceType = ApplianceType::VIDEO;

    // Status and sources
    DeviceStatus m_status = DeviceStatus::OFFLINE;
    bool m_isOnline = false;
    std::vector<DataSource> m_dataSources;
    std::vector<std::string> m_cameraCapabilities;

    // Ownership
    Ownership m_ownership = Ownership::UNKNOWN;

    // Timestamps
    std::chrono::system_clock::time_point m_createdTimestamp;
    std::chrono::system_clock::time_point m_updatedTimestamp;

    // Event type
    DeviceEventType m_eventType;

    // Event-specific fields
    std::vector<nlohmann::json> m_presets;
    std::vector<nlohmann::json> m_analyticEventInfo;
    std::vector<nlohmann::json> m_snapshotEventInfo;

   public:
    // Constructors
    MediaDeviceEvent(DeviceEventType type = DeviceEventType::PRESENCE);
    MediaDeviceEvent(const std::string& customerId, const std::string& applianceId, const std::string& deviceId,
        const std::string& deviceLabel, const std::string& ipAddress, const std::string& deviceStatus = "ONLINE",
        DeviceEventType type = DeviceEventType::PRESENCE);

    // Setters - Core identification
    void setCustomerId(const std::string& customerId) {
        m_customerId = customerId;
    }
    void setApplianceId(const std::string& applianceId) {
        m_applianceId = applianceId;
    }
    void setId(const std::string& id) {
        m_id = id;
    }
    void setLabel(const std::string& label) {
        m_label = label;
    }
    void setIpAddress(const std::string& ipAddress) {
        m_ipAddress = ipAddress;
    }

    // Setters - Device information
    void setManufacturer(const std::string& manufacturer) {
        m_manufacturer = manufacturer;
    }
    void setModelNumber(const std::string& modelNumber) {
        m_modelNumber = modelNumber;
    }
    void setSerialNumber(const std::string& serialNumber) {
        m_serialNumber = serialNumber;
    }
    void setMacAddress(const std::string& macAddress) {
        m_macAddress = macAddress;
    }
    void setFirmwareVersion(const std::string& firmwareVersion) {
        m_firmwareVersion = firmwareVersion;
    }

    // Setters - Device types and capabilities
    void setDeviceType(DeviceType type) {
        m_deviceType = type;
    }
    void setDeploymentType(DeploymentType type) {
        m_deploymentType = type;
    }
    void setApplianceType(ApplianceType type) {
        m_applianceType = type;
    }

    // Setters - Status and sources
    void setStatus(DeviceStatus status) {
        m_status = status;
    }
    void setIsOnline(bool online) {
        m_isOnline = online;
    }
    void setDataSources(const std::vector<DataSource>& sources) {
        m_dataSources = sources;
    }
    void setCameraCapabilities(const std::vector<std::string>& capabilities) {
        m_cameraCapabilities = capabilities;
    }

    // Setters - Ownership
    void setOwnership(Ownership ownership) {
        m_ownership = ownership;
    }

    // Setters - Event-specific fields
    void setPresets(const std::vector<nlohmann::json>& presets) {
        m_presets = presets;
    }
    void setAnalyticEventInfo(const std::vector<nlohmann::json>& info) {
        m_analyticEventInfo = info;
    }
    void setSnapshotEventInfo(const std::vector<nlohmann::json>& info) {
        m_snapshotEventInfo = info;
    }

    // Getters - Core identification
    std::string getCustomerId() const {
        return m_customerId;
    }
    std::string getApplianceId() const {
        return m_applianceId;
    }
    std::string getId() const {
        return m_id;
    }
    std::string getLabel() const {
        return m_label;
    }
    std::string getIpAddress() const {
        return m_ipAddress;
    }

    // Getters - Device information
    std::string getManufacturer() const {
        return m_manufacturer;
    }
    std::string getModelNumber() const {
        return m_modelNumber;
    }
    std::string getSerialNumber() const {
        return m_serialNumber;
    }
    std::string getMacAddress() const {
        return m_macAddress;
    }
    std::string getFirmwareVersion() const {
        return m_firmwareVersion;
    }

    // Getters - Device types and capabilities
    DeviceType getDeviceType() const {
        return m_deviceType;
    }
    DeploymentType getDeploymentType() const {
        return m_deploymentType;
    }
    ApplianceType getApplianceType() const {
        return m_applianceType;
    }

    // Getters - Status and sources
    DeviceStatus getStatus() const {
        return m_status;
    }
    bool getIsOnline() const {
        return m_isOnline;
    }
    const std::vector<DataSource>& getDataSources() const {
        return m_dataSources;
    }
    const std::vector<std::string>& getCameraCapabilities() const {
        return m_cameraCapabilities;
    }

    // Getters - Ownership
    Ownership getOwnership() const {
        return m_ownership;
    }

    // Getters - Timestamps
    std::chrono::system_clock::time_point getCreatedTimestamp() const {
        return m_createdTimestamp;
    }
    std::chrono::system_clock::time_point getUpdatedTimestamp() const {
        return m_updatedTimestamp;
    }

    // Getters - Event type
    DeviceEventType getEventType() const {
        return m_eventType;
    }

    // Getters - Event-specific fields
    const std::vector<nlohmann::json>& getPresets() const {
        return m_presets;
    }
    const std::vector<nlohmann::json>& getAnalyticEventInfo() const {
        return m_analyticEventInfo;
    }
    const std::vector<nlohmann::json>& getSnapshotEventInfo() const {
        return m_snapshotEventInfo;
    }

    // EventInterface implementation
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;

    // Public methods
    void updateTimestamp();
    void setOnlineStatus(bool online);
    void addDataSource(const DataSource& source);
    void addCapability(const std::string& capability);
    bool hasCapability(const std::string& capability) const;
    void setEventType(DeviceEventType type);
    void addPreset(const nlohmann::json& preset) {
        m_presets.push_back(preset);
    }
    void addAnalyticEvent(const nlohmann::json& event) {
        m_analyticEventInfo.push_back(event);
    }
    void addSnapshotEvent(const nlohmann::json& event) {
        m_snapshotEventInfo.push_back(event);
    }

    // Static utility methods
    static std::string toString(DeviceType type);
    static std::string toString(DeploymentType type);
    static std::string toString(ApplianceType type);
    static std::string toString(DeviceStatus status);
    static std::string toString(Ownership ownership);
    static std::string toString(DeviceEventType type);
    static std::string formatTimestamp(const std::chrono::system_clock::time_point& timePoint);

    static DeviceType deviceTypeFromString(const std::string& str);
    static DeploymentType deploymentTypeFromString(const std::string& str);
    static ApplianceType applianceTypeFromString(const std::string& str);
    static DeviceStatus deviceStatusFromString(const std::string& str);
    static Ownership ownershipFromString(const std::string& str);
    static DeviceEventType eventTypeFromString(const std::string& str);
};

}  // namespace media

// Backward compatibility aliases
using UnifiedMediaDeviceEvent = media::MediaDeviceEvent;
using MediaDeviceType = media::DeviceType;
using DataSourceType = media::DataSourceType;
using DataInterfaceRank = media::DataInterfaceRank;
using DeviceStatus = media::DeviceStatus;
using Ownership = media::Ownership;
using DataSource = media::DataSource;
using DataInterface = media::DataInterface;