#include "aegis/installer_dbus_client.h"

#include <cstdint>
#include <string>

namespace aegis {

InstallerDbusClient::InstallerDbusClient() = default;

InstallerDbusClient::~InstallerDbusClient() {
    if (connection_ != nullptr) {
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }
}

Result<void> InstallerDbusClient::connect_system_bus() {
    DBusError error;
    dbus_error_init(&error);

    connection_ = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (!connection_) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<void>::err("Failed to connect to system bus: " + msg);
    }

    return Result<void>::ok();
}

Result<void> InstallerDbusClient::subscribe_completed() {
    if (!connection_) {
        return Result<void>::err("D-Bus is not connected");
    }

    DBusError error;
    dbus_error_init(&error);

    dbus_bus_add_match(
        connection_,
        "type='signal',"
        "sender='de.pengutronix.aegis',"
        "path='/',"
        "interface='de.pengutronix.aegis.Installer',"
        "member='Completed'",
        &error);

    dbus_connection_flush(connection_);

    if (dbus_error_is_set(&error)) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<void>::err("Failed to subscribe to completion signal: " + msg);
    }

    return Result<void>::ok();
}

Result<void> InstallerDbusClient::install_bundle(const std::string& bundle_path,
                                                 bool ignore_compatible) {
    if (!connection_) {
        return Result<void>::err("D-Bus is not connected");
    }

    DBusMessage* message = dbus_message_new_method_call(
        kServiceName, kObjectPath, kInstallerInterface, "InstallBundle");
    if (!message) {
        return Result<void>::err("Failed to allocate D-Bus install message");
    }

    DBusMessageIter iter;
    DBusMessageIter dict;
    const char* source = bundle_path.c_str();

    dbus_message_iter_init_append(message, &iter);
    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &source)) {
        dbus_message_unref(message);
        return Result<void>::err("Failed to encode bundle path");
    }

    if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict)) {
        dbus_message_unref(message);
        return Result<void>::err("Failed to encode install args");
    }

    if (ignore_compatible) {
        DBusMessageIter entry;
        DBusMessageIter variant;
        const char* key = "ignore-compatible";
        dbus_bool_t value = TRUE;

        if (!dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry)) {
            dbus_message_unref(message);
            return Result<void>::err("Failed to open D-Bus dict entry");
        }

        if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key)) {
            dbus_message_unref(message);
            return Result<void>::err("Failed to append option key");
        }

        if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant)) {
            dbus_message_unref(message);
            return Result<void>::err("Failed to open D-Bus variant");
        }

        if (!dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value)) {
            dbus_message_unref(message);
            return Result<void>::err("Failed to append boolean option");
        }

        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }

    dbus_message_iter_close_container(&iter, &dict);

    DBusError error;
    dbus_error_init(&error);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection_, message, -1, &error);
    dbus_message_unref(message);

    if (!reply) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<void>::err("Install request failed: " + msg);
    }

    dbus_message_unref(reply);
    return Result<void>::ok();
}

Result<std::string> InstallerDbusClient::get_string_property(const char* property_name) {
    if (!connection_) {
        return Result<std::string>::err("D-Bus is not connected");
    }

    DBusMessage* message = dbus_message_new_method_call(
        kServiceName, kObjectPath, kPropertiesInterface, "Get");
    if (!message) {
        return Result<std::string>::err("Failed to allocate D-Bus property request");
    }

    const char* interface_name = kInstallerInterface;
    const char* property = property_name;

    if (!dbus_message_append_args(message,
                                  DBUS_TYPE_STRING, &interface_name,
                                  DBUS_TYPE_STRING, &property,
                                  DBUS_TYPE_INVALID)) {
        dbus_message_unref(message);
        return Result<std::string>::err("Failed to encode D-Bus property request");
    }

    DBusError error;
    dbus_error_init(&error);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection_, message, -1, &error);
    dbus_message_unref(message);

    if (!reply) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<std::string>::err("Failed to read property: " + msg);
    }

    DBusMessageIter iter;
    if (!dbus_message_iter_init(reply, &iter) ||
        dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
        dbus_message_unref(reply);
        return Result<std::string>::err("Property reply had unexpected type");
    }

    DBusMessageIter variant;
    dbus_message_iter_recurse(&iter, &variant);
    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_STRING) {
        dbus_message_unref(reply);
        return Result<std::string>::err("Property reply was not a string");
    }

    const char* value = nullptr;
    dbus_message_iter_get_basic(&variant, &value);
    std::string result = value ? value : "";

    dbus_message_unref(reply);
    return Result<std::string>::ok(result);
}

Result<std::string> InstallerDbusClient::get_last_error() {
    return get_string_property("LastError");
}

Result<int> InstallerDbusClient::wait_for_completed() {
    if (!connection_) {
        return Result<int>::err("D-Bus is not connected");
    }

    while (true) {
        dbus_connection_read_write(connection_, 1000);
        DBusMessage* message = dbus_connection_pop_message(connection_);
        if (!message) {
            continue;
        }

        if (dbus_message_is_signal(message, kInstallerInterface, "Completed")) {
            int32_t result = 1;
            DBusError error;
            dbus_error_init(&error);

            bool ok = dbus_message_get_args(message, &error,
                                            DBUS_TYPE_INT32, &result,
                                            DBUS_TYPE_INVALID);
            dbus_message_unref(message);

            if (!ok) {
                std::string msg = error.message ? error.message : "Unknown D-Bus error";
                dbus_error_free(&error);
                return Result<int>::err("Failed to read completion signal: " + msg);
            }

            return Result<int>::ok(result);
        }

        dbus_message_unref(message);
    }
}

} // namespace aegis