#pragma once
#include <nlohmann/json.hpp>
#include <string>

#include "EventInterface.h"

class EdgeApplianceEventType {
   public:
    enum class Type {
        CAMERADISCOVERY,
        CAMERAPRESENCE,
        CAMERAUPDATE,
        CAMERADELETE,
        CAMERASTREAMINGSTATE,
        CAMERAEVENT,
        DEVICEEVENT,
        DISCOVERYSTATUS,
        FAULTEVENT,
        UNKNOWN
    };

    static const char* toString(Type type) {
        switch (type) {
            case Type::CAMERADISCOVERY:
                return "CAMERADISCOVERY";
            case Type::CAMERAPRESENCE:
                return "CAMERAPRESENCE";
            case Type::CAMERAUPDATE:
                return "CAMERAUPDATE";
            case Type::CAMERADELETE:
                return "CAMERADELETE";
            case Type::CAMERASTREAMINGSTATE:
                return "CAMERASTREAMINGSTATE";
            case Type::CAMERAEVENT:
                return "CAMERAEVENT";
            case Type::DEVICEEVENT:
                return "DEVICEEVENT";
            case Type::DISCOVERYSTATUS:
                return "DISCOVERYSTATUS";
            case Type::FAULTEVENT:
                return "FAULTEVENT";
            case Type::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    static Type fromString(const std::string& str) {
        if (str == "CAMERADISCOVERY")
            return Type::CAMERADISCOVERY;
        if (str == "CAMERAPRESENCE")
            return Type::CAMERAPRESENCE;
        if (str == "CAMERAUPDATE")
            return Type::CAMERAUPDATE;
        if (str == "CAMERADELETE")
            return Type::CAMERADELETE;
        if (str == "CAMERASTREAMINGSTATE")
            return Type::CAMERASTREAMINGSTATE;
        if (str == "CAMERAEVENT")
            return Type::CAMERAEVENT;
        if (str == "DEVICEEVENT")
            return Type::DEVICEEVENT;
        if (str == "DISCOVERYSTATUS")
            return Type::DISCOVERYSTATUS;
        return Type::UNKNOWN;
    }
};

class EdgeApplianceEventRecord : public EventInterface {
   public:
    EdgeApplianceEventRecord();

    // Setters
    void setObjectType(EdgeApplianceEventType::Type type);
    void setPayload(const std::string& payload);
    void setSender(const std::string& sender);

    // Override from EventInterface
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;

    // Utility method
    std::string toString() const {
        return toJson().dump();
    }

    // Getters
    EdgeApplianceEventType::Type getObjectType() const {
        return m_objectType;
    }
    std::string getPayload() const {
        return m_payload;
    }
    std::string getSender() const {
        return m_sender;
    }

    bool isObjectTypeSet() const {
        return m_is_object_type_set;
    }
    bool isPayloadSet() const {
        return m_is_payload_set;
    }
    bool isSenderSet() const {
        return m_is_sender_set;
    }

   private:
    EdgeApplianceEventType::Type m_objectType;
    std::string m_payload;
    std::string m_sender;
    bool m_is_sender_set;
    bool m_is_object_type_set;
    bool m_is_payload_set;
};