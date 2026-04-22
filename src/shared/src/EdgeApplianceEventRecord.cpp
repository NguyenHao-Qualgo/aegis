#include "EdgeApplianceEventRecord.h"

EdgeApplianceEventRecord::EdgeApplianceEventRecord()
    : m_objectType(EdgeApplianceEventType::Type::UNKNOWN),
      m_payload(""),
      m_sender(""),
      m_is_sender_set(false),
      m_is_object_type_set(false),
      m_is_payload_set(false) {
}

void EdgeApplianceEventRecord::setObjectType(EdgeApplianceEventType::Type type) {
    m_objectType = type;
    m_is_object_type_set = true;
}

void EdgeApplianceEventRecord::setPayload(const std::string& payload) {
    m_payload = payload;
    m_is_payload_set = true;
}

void EdgeApplianceEventRecord::setSender(const std::string& sender) {
    m_sender = sender;
    m_is_sender_set = true;
}

nlohmann::json EdgeApplianceEventRecord::toJson() const {
    nlohmann::json json;
    if (m_is_object_type_set) {
        json["objectType"] = EdgeApplianceEventType::toString(m_objectType);
    }
    if (m_is_payload_set) {
        json["payload"] = m_payload;
    }
    if (m_is_sender_set) {
        json["sender"] = m_sender;
    }
    return json;
}

bool EdgeApplianceEventRecord::fromJson(const nlohmann::json& json) {
    try {
        if (json.contains("objectType") && !json["objectType"].is_null()) {
            setObjectType(EdgeApplianceEventType::fromString(json["objectType"].get<std::string>()));
        }
        if (json.contains("payload") && !json["payload"].is_null()) {
            setPayload(json["payload"].get<std::string>());
        }
        if (json.contains("sender") && !json["sender"].is_null()) {
            setSender(json["sender"].get<std::string>());
        }
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}