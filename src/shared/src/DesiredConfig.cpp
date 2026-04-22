#include "DesiredConfig.h"

DesiredConfig::DesiredConfig()
    : m_customerIdIsSet(false),
      m_applianceIdIsSet(false),
      m_applianceTypeIsSet(false),
      m_serialNumberIsSet(false),
      m_applianceAliasIsSet(false) {
}

DesiredConfig::DesiredConfig(const nlohmann::json& json) : DesiredConfig() {
    fromJson(json);
}

void DesiredConfig::setCustomerId(const std::string& customerId) {
    m_customerId = customerId;
    m_customerIdIsSet = true;
}

void DesiredConfig::setApplianceId(const std::string& applianceId) {
    m_applianceId = applianceId;
    m_applianceIdIsSet = true;
}

void DesiredConfig::setApplianceType(const std::string& applianceType) {
    m_applianceType = applianceType;
    m_applianceTypeIsSet = true;
}

void DesiredConfig::setSerialNumber(const std::string& serialNumber) {
    m_serialNumber = serialNumber;
    m_serialNumberIsSet = true;
}

void DesiredConfig::setApplianceAlias(const std::string& applianceAlias) {
    m_applianceAlias = applianceAlias;
    m_applianceAliasIsSet = true;
}

void DesiredConfig::addCameraCredentialConfig(const CameraCredentialConfig& config) {
    m_cameraCredentialConfigs.push_back(config);
}

void DesiredConfig::setCameraCredentialConfigs(const std::vector<CameraCredentialConfig>& configs) {
    m_cameraCredentialConfigs = configs;
}

CameraCredentialConfig& DesiredConfig::getCameraCredentialConfig(size_t index) {
    // Ensure we have at least one config
    if (m_cameraCredentialConfigs.empty()) {
        m_cameraCredentialConfigs.emplace_back();
    }

    // Return requested index if valid, otherwise return first
    if (index < m_cameraCredentialConfigs.size()) {
        return m_cameraCredentialConfigs[index];
    }
    return m_cameraCredentialConfigs[0];
}

nlohmann::json DesiredConfig::toJson() const {
    nlohmann::json j;

    if (m_customerIdIsSet) {
        j["customerId"] = m_customerId;
    }

    if (m_applianceIdIsSet) {
        j["applianceId"] = m_applianceId;
    }

    if (m_applianceTypeIsSet) {
        j["applianceType"] = m_applianceType;
    }

    if (m_serialNumberIsSet) {
        j["serialNumber"] = m_serialNumber;
    }

    if (m_applianceAliasIsSet) {
        j["applianceAlias"] = m_applianceAlias;
    }

    if (!m_cameraCredentialConfigs.empty()) {
        nlohmann::json configsArray = nlohmann::json::array();
        for (const auto& config : m_cameraCredentialConfigs) {
            configsArray.push_back(config.toJson());
        }
        j["cameraCredentialConfigs"] = configsArray;
    }

    return j;
}

bool DesiredConfig::fromJson(const nlohmann::json& json) {
    if (!json.is_object()) {
        return false;
    }

    try {
        if (json.contains("customerId") && json["customerId"].is_string()) {
            setCustomerId(json["customerId"].get<std::string>());
        }

        if (json.contains("applianceId") && json["applianceId"].is_string()) {
            setApplianceId(json["applianceId"].get<std::string>());
        }

        if (json.contains("applianceType") && json["applianceType"].is_string()) {
            setApplianceType(json["applianceType"].get<std::string>());
        }

        if (json.contains("serialNumber") && json["serialNumber"].is_string()) {
            setSerialNumber(json["serialNumber"].get<std::string>());
        }

        if (json.contains("applianceAlias") && json["applianceAlias"].is_string()) {
            setApplianceAlias(json["applianceAlias"].get<std::string>());
        }

        if (json.contains("cameraCredentialConfigs") && json["cameraCredentialConfigs"].is_array()) {
            m_cameraCredentialConfigs.clear();
            for (const auto& configJson : json["cameraCredentialConfigs"]) {
                CameraCredentialConfig config;
                if (config.fromJson(configJson)) {
                    addCameraCredentialConfig(config);
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}