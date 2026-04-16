#pragma once

#include <array>

namespace aegis::dbus {

inline constexpr const char* kServiceName = "de.pengutronix.aegis";
inline constexpr const char* kObjectPath = "/";
inline constexpr const char* kInstallerInterface = "de.pengutronix.aegis.Installer";
inline constexpr const char* kPropertiesInterface = "org.freedesktop.DBus.Properties";
inline constexpr const char* kIntrospectableInterface = "org.freedesktop.DBus.Introspectable";

inline constexpr std::array kInstallerProperties = {
    "Operation", "LastError", "Progress",       "Compatible",
    "Variant",   "BootSlot",  "ServiceVersion", "Bootloader",
    "OtaState", "OtaStatusMessage", "TransactionId", "ExpectedSlot",
};

} // namespace aegis::dbus
