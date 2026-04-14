#pragma once

#include "aegis/utils.h"

#include <dbus/dbus.h>
#include <string>

namespace aegis {

class InstallerDbusClient {
public:
    InstallerDbusClient();
    ~InstallerDbusClient();

    InstallerDbusClient(const InstallerDbusClient&) = delete;
    InstallerDbusClient& operator=(const InstallerDbusClient&) = delete;

    Result<void> connect_system_bus();
    Result<void> subscribe_completed();
    Result<void> install_bundle(const std::string& bundle_path, bool ignore_compatible);
    Result<int> wait_for_completed();
    Result<std::string> get_last_error();

private:
    Result<std::string> get_string_property(const char* property_name);

    DBusConnection* connection_ = nullptr;

    static constexpr const char* kServiceName = "de.pengutronix.aegis";
    static constexpr const char* kObjectPath = "/";
    static constexpr const char* kInstallerInterface = "de.pengutronix.aegis.Installer";
    static constexpr const char* kPropertiesInterface = "org.freedesktop.DBus.Properties";
};

} // namespace aegis