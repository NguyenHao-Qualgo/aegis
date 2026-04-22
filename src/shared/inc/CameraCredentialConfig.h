#pragma once
#include <nlohmann/json.hpp>
#include <string>

#include "EventInterface.h"

class CameraCredentialConfig : public EventInterface {
   private:
    std::string m_username;
    std::string m_password;
    std::string m_ipAddressEnd;
    std::string m_ipAddressStart;

    // IsSet flags
    bool m_usernameIsSet;
    bool m_passwordIsSet;
    bool m_ipAddressEndIsSet;
    bool m_ipAddressStartIsSet;

   public:
    CameraCredentialConfig();

    // Setters
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setIpAddressEnd(const std::string& ipAddressEnd);
    void setIpAddressStart(const std::string& ipAddressStart);

    // Getters
    std::string getUsername() const {
        return m_username;
    }
    std::string getPassword() const {
        return m_password;
    }
    std::string getIpAddressEnd() const {
        return m_ipAddressEnd;
    }
    std::string getIpAddressStart() const {
        return m_ipAddressStart;
    }

    // IsSet getters
    bool isUsernameSet() const {
        return m_usernameIsSet;
    }
    bool isPasswordSet() const {
        return m_passwordIsSet;
    }
    bool isIpAddressEndSet() const {
        return m_ipAddressEndIsSet;
    }
    bool isIpAddressStartSet() const {
        return m_ipAddressStartIsSet;
    }

    // Utility methods
    bool isValid() const;
    bool isEmpty() const;
    void clear();

    // EventInterface implementation
    nlohmann::json toJson() const override;
    bool fromJson(const nlohmann::json& json) override;
};