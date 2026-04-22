#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

#include "EventInterface.h"

class FaultEvent : public EventInterface {
   private:
    std::string m_uuid;
    std::string m_customerId;
    std::string m_applianceId;
    std::string m_applianceSerialNumber;
    std::string m_applianceType;
    std::string m_applianceAlias;
    std::string m_description;
    std::string m_severity;
    std::string m_status;
    std::string m_sourceService;
    int64_t m_sourcedOn{0};

    bool m_uuidIsSet{false};
    bool m_customerIdIsSet{false};
    bool m_applianceIdIsSet{false};
    bool m_applianceSerialNumberIsSet{false};
    bool m_applianceTypeIsSet{false};
    bool m_applianceAliasIsSet{false};
    bool m_descriptionIsSet{false};
    bool m_severityIsSet{false};
    bool m_statusIsSet{false};
    bool m_sourceServiceIsSet{false};
    bool m_sourcedOnIsSet{false};

   public:
    FaultEvent() = default;

    void setUuid(const std::string& value) {
        m_uuid = value;
        m_uuidIsSet = true;
    }
    void setCustomerId(const std::string& value) {
        m_customerId = value;
        m_customerIdIsSet = true;
    }
    void setApplianceId(const std::string& value) {
        m_applianceId = value;
        m_applianceIdIsSet = true;
    }
    void setApplianceSerialNumber(const std::string& value) {
        m_applianceSerialNumber = value;
        m_applianceSerialNumberIsSet = true;
    }
    void setApplianceType(const std::string& value) {
        m_applianceType = value;
        m_applianceTypeIsSet = true;
    }
    void setApplianceAlias(const std::string& value) {
        m_applianceAlias = value;
        m_applianceAliasIsSet = true;
    }
    void setDescription(const std::string& value) {
        m_description = value;
        m_descriptionIsSet = true;
    }
    void setSeverity(const std::string& value) {
        m_severity = value;
        m_severityIsSet = true;
    }
    void setStatus(const std::string& value) {
        m_status = value;
        m_statusIsSet = true;
    }
    void setSourceService(const std::string& value) {
        m_sourceService = value;
        m_sourceServiceIsSet = true;
    }
    void setSourcedOn(int64_t value) {
        m_sourcedOn = value;
        m_sourcedOnIsSet = true;
    }

    nlohmann::json toJson() const override {
        nlohmann::json val;
        if (m_uuidIsSet)
            val["uuid"] = m_uuid;
        if (m_customerIdIsSet)
            val["customerId"] = m_customerId;
        if (m_applianceIdIsSet)
            val["applianceId"] = m_applianceId;
        if (m_applianceSerialNumberIsSet)
            val["applianceSerialNumber"] = m_applianceSerialNumber;
        if (m_applianceTypeIsSet)
            val["applianceType"] = m_applianceType;
        if (m_applianceAliasIsSet)
            val["applianceAlias"] = m_applianceAlias;
        if (m_descriptionIsSet)
            val["description"] = m_description;
        if (m_severityIsSet)
            val["severity"] = m_severity;
        if (m_statusIsSet)
            val["status"] = m_status;
        if (m_sourceServiceIsSet)
            val["sourceService"] = m_sourceService;
        if (m_sourcedOnIsSet)
            val["sourcedOn"] = m_sourcedOn;
        return val;
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            if (json.contains("uuid") && !json["uuid"].is_null())
                setUuid(json["uuid"].get<std::string>());
            if (json.contains("customerId") && !json["customerId"].is_null())
                setCustomerId(json["customerId"].get<std::string>());
            if (json.contains("applianceId") && !json["applianceId"].is_null())
                setApplianceId(json["applianceId"].get<std::string>());
            if (json.contains("applianceSerialNumber") && !json["applianceSerialNumber"].is_null())
                setApplianceSerialNumber(json["applianceSerialNumber"].get<std::string>());
            if (json.contains("applianceType") && !json["applianceType"].is_null())
                setApplianceType(json["applianceType"].get<std::string>());
            if (json.contains("applianceAlias") && !json["applianceAlias"].is_null())
                setApplianceAlias(json["applianceAlias"].get<std::string>());
            if (json.contains("description") && !json["description"].is_null())
                setDescription(json["description"].get<std::string>());
            if (json.contains("severity") && !json["severity"].is_null())
                setSeverity(json["severity"].get<std::string>());
            if (json.contains("status") && !json["status"].is_null())
                setStatus(json["status"].get<std::string>());
            if (json.contains("sourceService") && !json["sourceService"].is_null())
                setSourceService(json["sourceService"].get<std::string>());
            if (json.contains("sourcedOn") && !json["sourcedOn"].is_null())
                setSourcedOn(json["sourcedOn"].get<int64_t>());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
};