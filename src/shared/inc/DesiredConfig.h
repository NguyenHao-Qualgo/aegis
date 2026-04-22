#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "CameraCredentialConfig.h"
#include "EventInterface.h"

class DesiredConfig : public EventInterface {
   private:
    std::string m_customerId;
    std::string m_applianceId;
    std::string m_applianceType;
    std::string m_serialNumber;
    std::string m_applianceAlias;
    std::vector<CameraCredentialConfig> m_cameraCredentialConfigs;

    // IsSet flags
    bool m_customerIdIsSet;
    bool m_applianceIdIsSet;
    bool m_applianceTypeIsSet;
    bool m_serialNumberIsSet;
    bool m_applianceAliasIsSet;

   public:
    DesiredConfig();
    explicit DesiredConfig(const nlohmann::json& json);

    // Setters
    void setCustomerId(const std::string& customerId);
    void setApplianceId(const std::string& applianceId);
    void setApplianceType(const std::string& applianceType);
    void setSerialNumber(const std::string& serialNumber);
    void setApplianceAlias(const std::string& applianceAlias);
    void addCameraCredentialConfig(const CameraCredentialConfig& config);
    void setCameraCredentialConfigs(const std::vector<CameraCredentialConfig>& configs);

    // Getters
    std::string getCustomerId() const {
        return m_customerId;
    }
    std::string getApplianceId() const {
        return m_applianceId;
    }
    std::string getApplianceType() const {
        return m_applianceType;
    }
    std::string getSerialNumber() const {
        return m_serialNumber;
    }
    std::string getApplianceAlias() const {
        return m_applianceAlias;
    }
    const std::vector<CameraCredentialConfig>& getCameraCredentialConfigs() const {
        return m_cameraCredentialConfigs;
    }
    std::vector<CameraCredentialConfig>& getCameraCredentialConfigs() {
        return m_cameraCredentialConfigs;
    }

    // Utility methods
    CameraCredentialConfig& getCameraCredentialConfig(size_t index = 0);
    bool hasCameraCredentialConfig() const {
        return !m_cameraCredentialConfigs.empty();
    }
    size_t getCameraCredentialConfigCount() const {
        return m_cameraCredentialConfigs.size();
    }

    // EventInterface implementation
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;

    // Legacy method for compatibility
    void UpdateFromJson(const nlohmann::json& json) {
        fromJson(json);
    }
};