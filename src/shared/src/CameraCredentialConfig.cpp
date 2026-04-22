#include "CameraCredentialConfig.h"

CameraCredentialConfig::CameraCredentialConfig()
    : m_usernameIsSet(false), m_passwordIsSet(false), m_ipAddressEndIsSet(false), m_ipAddressStartIsSet(false) {
}

void CameraCredentialConfig::setUsername(const std::string& username) {
    m_username = username;
    m_usernameIsSet = true;
}

void CameraCredentialConfig::setPassword(const std::string& password) {
    m_password = password;
    m_passwordIsSet = true;
}

void CameraCredentialConfig::setIpAddressEnd(const std::string& ipAddressEnd) {
    m_ipAddressEnd = ipAddressEnd;
    m_ipAddressEndIsSet = true;
}

void CameraCredentialConfig::setIpAddressStart(const std::string& ipAddressStart) {
    m_ipAddressStart = ipAddressStart;
    m_ipAddressStartIsSet = true;
}

bool CameraCredentialConfig::isValid() const {
    return m_usernameIsSet && !m_username.empty() && m_passwordIsSet && !m_password.empty() && m_ipAddressEndIsSet &&
           !m_ipAddressEnd.empty() && m_ipAddressStartIsSet && !m_ipAddressStart.empty();
}

bool CameraCredentialConfig::isEmpty() const {
    return (!m_usernameIsSet || m_username.empty()) && (!m_passwordIsSet || m_password.empty()) &&
           (!m_ipAddressEndIsSet || m_ipAddressEnd.empty()) && (!m_ipAddressStartIsSet || m_ipAddressStart.empty());
}

void CameraCredentialConfig::clear() {
    m_username.clear();
    m_password.clear();
    m_ipAddressEnd.clear();
    m_ipAddressStart.clear();
    m_usernameIsSet = false;
    m_passwordIsSet = false;
    m_ipAddressEndIsSet = false;
    m_ipAddressStartIsSet = false;
}

nlohmann::json CameraCredentialConfig::toJson() const {
    nlohmann::json j;

    if (m_usernameIsSet) {
        j["username"] = m_username;
    }

    if (m_passwordIsSet) {
        j["password"] = m_password;
    }

    if (m_ipAddressEndIsSet) {
        j["ipAddressEnd"] = m_ipAddressEnd;
    }

    if (m_ipAddressStartIsSet) {
        j["ipAddressStart"] = m_ipAddressStart;
    }

    return j;
}

bool CameraCredentialConfig::fromJson(const nlohmann::json& json) {
    if (!json.is_object()) {
        return false;
    }

    try {
        // Clear existing data
        clear();

        // Parse each field if present
        if (json.contains("username") && json["username"].is_string()) {
            setUsername(json["username"].get<std::string>());
        }

        if (json.contains("password") && json["password"].is_string()) {
            setPassword(json["password"].get<std::string>());
        }

        if (json.contains("ipAddressEnd") && json["ipAddressEnd"].is_string()) {
            setIpAddressEnd(json["ipAddressEnd"].get<std::string>());
        }

        if (json.contains("ipAddressStart") && json["ipAddressStart"].is_string()) {
            setIpAddressStart(json["ipAddressStart"].get<std::string>());
        }

        // Return true if we successfully parsed the JSON
        // The original implementation required all fields to be present
        // This implementation is more flexible and returns true if parsing succeeded
        return true;

    } catch (const std::exception& e) {
        return false;
    }
}