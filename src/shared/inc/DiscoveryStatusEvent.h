#pragma once
#include <chrono>
#include <string>

#include "EventInterface.h"

class DiscoveryStatusEvent : public EventInterface {
   private:
    std::string m_applianceId;
    std::string m_status;
    int m_discoveredDevices;
    int m_processedDevices;
    std::chrono::milliseconds m_eventTime;

    bool m_applianceIdIsSet;
    bool m_statusIsSet;
    bool m_discoveredDevicesIsSet;
    bool m_processedDevicesIsSet;
    bool m_eventTimeIsSet;

   public:
    DiscoveryStatusEvent()
        : m_discoveredDevices(0),
          m_processedDevices(0),
          m_applianceIdIsSet(false),
          m_statusIsSet(false),
          m_discoveredDevicesIsSet(false),
          m_processedDevicesIsSet(false),
          m_eventTimeIsSet(false) {
        setEventTime(getCurrentTimestamp());
    }

    // Setters
    void setApplianceId(const std::string& value) {
        m_applianceId = value;
        m_applianceIdIsSet = true;
    }
    void setStatus(const std::string& value) {
        m_status = value;
        m_statusIsSet = true;
    }
    void setDiscoveredDevices(int value) {
        m_discoveredDevices = value;
        m_discoveredDevicesIsSet = true;
    }
    void setProcessedDevices(int value) {
        m_processedDevices = value;
        m_processedDevicesIsSet = true;
    }
    void setEventTime(std::chrono::milliseconds value) {
        m_eventTime = value;
        m_eventTimeIsSet = true;
    }

    nlohmann::json toJson() const override {
        nlohmann::json val;

        if (m_applianceIdIsSet) {
            val["applianceId"] = m_applianceId;
        }
        if (m_statusIsSet) {
            val["status"] = m_status;
        }
        if (m_discoveredDevicesIsSet) {
            val["discoveredDevices"] = m_discoveredDevices;
        }
        if (m_processedDevicesIsSet) {
            val["processedDevices"] = m_processedDevices;
        }
        if (m_eventTimeIsSet) {
            val["eventTime"] = m_eventTime.count();
        }

        return val;
    }

    bool fromJson(const nlohmann::json& json) override {
        try {
            if (json.contains("applianceId") && !json["applianceId"].is_null()) {
                setApplianceId(json["applianceId"].get<std::string>());
            }
            if (json.contains("status") && !json["status"].is_null()) {
                setStatus(json["status"].get<std::string>());
            }
            if (json.contains("discoveredDevices") && !json["discoveredDevices"].is_null()) {
                setDiscoveredDevices(json["discoveredDevices"].get<int>());
            }
            if (json.contains("processedDevices") && !json["processedDevices"].is_null()) {
                setProcessedDevices(json["processedDevices"].get<int>());
            }
            if (json.contains("eventTime") && !json["eventTime"].is_null()) {
                setEventTime(std::chrono::milliseconds(json["eventTime"].get<int64_t>()));
            }
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
};