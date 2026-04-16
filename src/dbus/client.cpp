#include "aegis/dbus/client.h"
#include "aegis/dbus/interface.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

namespace aegis {

AegisDbusClient::AegisDbusClient() = default;

AegisDbusClient::~AegisDbusClient() {
    if (connection_) {
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }
}

Result<void> AegisDbusClient::connect_system_bus() {
    DBusError error;
    dbus_error_init(&error);

    DBusBusType bus_type = DBUS_BUS_SYSTEM;
    if (const char* bus_env = std::getenv("AEGIS_DBUS_BUS")) {
        if (std::string(bus_env) == "session") {
            bus_type = DBUS_BUS_SESSION;
        }
    }

    connection_ = dbus_bus_get(bus_type, &error);
    if (!connection_) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<void>::err("Failed to connect to D-Bus bus: " + msg);
    }

    return Result<void>::ok();
}

Result<void> AegisDbusClient::subscribe_completed() {
    if (!connection_) {
        return Result<void>::err("D-Bus is not connected");
    }

    DBusError error;
    dbus_error_init(&error);

    dbus_bus_add_match(connection_,
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

Result<DBusMessage*> AegisDbusClient::call_method(DBusMessage* message) {
    if (!connection_) {
        if (message) {
            dbus_message_unref(message);
        }
        return Result<DBusMessage*>::err("D-Bus is not connected");
    }

    DBusError error;
    dbus_error_init(&error);

    DBusMessage* reply =
        dbus_connection_send_with_reply_and_block(connection_, message, -1, &error);
    dbus_message_unref(message);

    if (!reply) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<DBusMessage*>::err("D-Bus method call failed: " + msg);
    }

    return Result<DBusMessage*>::ok(reply);
}

Result<DBusMessage*> AegisDbusClient::call_properties_get(const char* property_name) {
    DBusMessage* message = dbus_message_new_method_call(dbus::kServiceName, dbus::kObjectPath,
                                                        dbus::kPropertiesInterface, "Get");
    if (!message) {
        return Result<DBusMessage*>::err("Failed to allocate D-Bus property request");
    }

    const char* iface = dbus::kInstallerInterface;
    const char* prop = property_name;
    if (!dbus_message_append_args(message, DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop,
                                  DBUS_TYPE_INVALID)) {
        dbus_message_unref(message);
        return Result<DBusMessage*>::err("Failed to encode D-Bus property request");
    }

    return call_method(message);
}

Result<std::string> AegisDbusClient::get_property_string(const char* property_name) {
    auto reply_res = call_properties_get(property_name);
    if (!reply_res) {
        return Result<std::string>::err(reply_res.error());
    }

    DBusMessage* reply = reply_res.value();

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

Result<ProgressInfo> AegisDbusClient::get_progress() {
    auto reply_res = call_properties_get("Progress");
    if (!reply_res) {
        return Result<ProgressInfo>::err(reply_res.error());
    }

    DBusMessage* reply = reply_res.value();

    DBusMessageIter iter;
    if (!dbus_message_iter_init(reply, &iter) ||
        dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
        dbus_message_unref(reply);
        return Result<ProgressInfo>::err("Progress reply had unexpected type");
    }

    DBusMessageIter variant;
    dbus_message_iter_recurse(&iter, &variant);
    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_STRUCT) {
        dbus_message_unref(reply);
        return Result<ProgressInfo>::err("Progress reply was not a struct");
    }

    DBusMessageIter s;
    dbus_message_iter_recurse(&variant, &s);

    ProgressInfo info{};

    if (dbus_message_iter_get_arg_type(&s) != DBUS_TYPE_INT32) {
        dbus_message_unref(reply);
        return Result<ProgressInfo>::err("Progress.percentage missing");
    }
    dbus_int32_t percentage = 0;
    dbus_message_iter_get_basic(&s, &percentage);
    info.percentage = static_cast<int>(percentage);

    if (!dbus_message_iter_next(&s) || dbus_message_iter_get_arg_type(&s) != DBUS_TYPE_STRING) {
        dbus_message_unref(reply);
        return Result<ProgressInfo>::err("Progress.message missing");
    }
    const char* message = nullptr;
    dbus_message_iter_get_basic(&s, &message);
    info.message = message ? message : "";

    if (!dbus_message_iter_next(&s) || dbus_message_iter_get_arg_type(&s) != DBUS_TYPE_INT32) {
        dbus_message_unref(reply);
        return Result<ProgressInfo>::err("Progress.depth missing");
    }
    dbus_int32_t depth = 0;
    dbus_message_iter_get_basic(&s, &depth);
    info.depth = static_cast<int>(depth);

    dbus_message_unref(reply);
    return Result<ProgressInfo>::ok(info);
}

Result<void> AegisDbusClient::install_bundle(const std::string& bundle_path,
                                             bool ignore_compatible) {
    DBusMessage* message = dbus_message_new_method_call(dbus::kServiceName, dbus::kObjectPath,
                                                        dbus::kInstallerInterface, "InstallBundle");
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

        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }

    dbus_message_iter_close_container(&iter, &dict);

    auto reply_res = call_method(message);
    if (!reply_res) {
        return Result<void>::err(reply_res.error());
    }

    dbus_message_unref(reply_res.value());
    return Result<void>::ok();
}


Result<int> AegisDbusClient::install_bundle_with_progress(const std::string& bundle_path,
                                                          bool ignore_compatible) {
    auto subscribe = subscribe_completed();
    if (!subscribe) {
        return Result<int>::err(subscribe.error());
    }

    auto install = install_bundle(bundle_path, ignore_compatible);
    if (!install) {
        return Result<int>::err(install.error());
    }

    std::cout << "Install request sent to Aegis service." << std::endl;

    int last_percent = -1;
    std::string last_message;

    while (true) {
        dbus_connection_read_write(connection_, 200);
        while (DBusMessage* message = dbus_connection_pop_message(connection_)) {
            if (dbus_message_is_signal(message, dbus::kInstallerInterface, "Completed")) {
                dbus_int32_t result = 1;
                DBusError error;
                dbus_error_init(&error);

                bool ok = dbus_message_get_args(message, &error, DBUS_TYPE_INT32, &result,
                                                DBUS_TYPE_INVALID);
                dbus_message_unref(message);
                if (!ok) {
                    std::string msg = error.message ? error.message : "Unknown D-Bus error";
                    dbus_error_free(&error);
                    return Result<int>::err("Failed to read completion signal: " + msg);
                }

                return Result<int>::ok(static_cast<int>(result));
            }

            dbus_message_unref(message);
        }

        auto progress = get_progress();
        if (progress) {
            const auto& p = progress.value();
            if (p.percentage != last_percent || p.message != last_message) {
                std::cout << "[" << std::setw(3) << p.percentage << "%] " << p.message
                          << std::endl;
                last_percent = p.percentage;
                last_message = p.message;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

Result<int> AegisDbusClient::wait_for_completed() {
    if (!connection_) {
        return Result<int>::err("D-Bus is not connected");
    }

    while (true) {
        dbus_connection_read_write(connection_, 1000);
        DBusMessage* message = dbus_connection_pop_message(connection_);
        if (!message) {
            continue;
        }

        if (dbus_message_is_signal(message, dbus::kInstallerInterface, "Completed")) {
            dbus_int32_t result = 1;
            DBusError error;
            dbus_error_init(&error);

            bool ok =
                dbus_message_get_args(message, &error, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
            dbus_message_unref(message);

            if (!ok) {
                std::string msg = error.message ? error.message : "Unknown D-Bus error";
                dbus_error_free(&error);
                return Result<int>::err("Failed to read completion signal: " + msg);
            }

            return Result<int>::ok(static_cast<int>(result));
        }

        dbus_message_unref(message);
    }
}

Result<std::string> AegisDbusClient::get_primary() {
    DBusMessage* message = dbus_message_new_method_call(dbus::kServiceName, dbus::kObjectPath,
                                                        dbus::kInstallerInterface, "GetPrimary");
    if (!message) {
        return Result<std::string>::err("Failed to allocate GetPrimary message");
    }

    auto reply_res = call_method(message);
    if (!reply_res) {
        return Result<std::string>::err(reply_res.error());
    }

    DBusMessage* reply = reply_res.value();
    const char* value = nullptr;

    if (!dbus_message_get_args(reply, nullptr, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID)) {
        dbus_message_unref(reply);
        return Result<std::string>::err("Invalid GetPrimary reply");
    }

    std::string result = value ? value : "";
    dbus_message_unref(reply);
    return Result<std::string>::ok(result);
}

Result<MarkResult> AegisDbusClient::mark(const std::string& state,
                                         const std::string& slot_identifier) {
    DBusMessage* message = dbus_message_new_method_call(dbus::kServiceName, dbus::kObjectPath,
                                                        dbus::kInstallerInterface, "Mark");
    if (!message) {
        return Result<MarkResult>::err("Failed to allocate Mark message");
    }

    const char* state_c = state.c_str();
    const char* slot_c = slot_identifier.c_str();

    if (!dbus_message_append_args(message, DBUS_TYPE_STRING, &state_c, DBUS_TYPE_STRING, &slot_c,
                                  DBUS_TYPE_INVALID)) {
        dbus_message_unref(message);
        return Result<MarkResult>::err("Failed to encode Mark request");
    }

    auto reply_res = call_method(message);
    if (!reply_res) {
        return Result<MarkResult>::err(reply_res.error());
    }

    DBusMessage* reply = reply_res.value();

    const char* slot_name = nullptr;
    const char* msg = nullptr;
    if (!dbus_message_get_args(reply, nullptr, DBUS_TYPE_STRING, &slot_name, DBUS_TYPE_STRING, &msg,
                               DBUS_TYPE_INVALID)) {
        dbus_message_unref(reply);
        return Result<MarkResult>::err("Invalid Mark reply");
    }

    MarkResult result;
    result.slot_name = slot_name ? slot_name : "";
    result.message = msg ? msg : "";

    dbus_message_unref(reply);
    return Result<MarkResult>::ok(result);
}

Result<std::vector<SlotStatusView>> AegisDbusClient::get_slot_status() {
    DBusMessage* message = dbus_message_new_method_call(dbus::kServiceName, dbus::kObjectPath,
                                                        dbus::kInstallerInterface, "GetSlotStatus");
    if (!message) {
        return Result<std::vector<SlotStatusView>>::err("Failed to allocate GetSlotStatus message");
    }

    auto reply_res = call_method(message);
    if (!reply_res) {
        return Result<std::vector<SlotStatusView>>::err(reply_res.error());
    }

    DBusMessage* reply = reply_res.value();
    DBusMessageIter iter;

    if (!dbus_message_iter_init(reply, &iter) ||
        dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
        dbus_message_unref(reply);
        return Result<std::vector<SlotStatusView>>::err("GetSlotStatus reply had unexpected type");
    }

    DBusMessageIter array_iter;
    dbus_message_iter_recurse(&iter, &array_iter);

    std::vector<SlotStatusView> out;

    while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_STRUCT) {
            dbus_message_unref(reply);
            return Result<std::vector<SlotStatusView>>::err("Invalid slot status entry");
        }

        DBusMessageIter struct_iter;
        dbus_message_iter_recurse(&array_iter, &struct_iter);

        SlotStatusView view;

        const char* slot_name = nullptr;
        dbus_message_iter_get_basic(&struct_iter, &slot_name);
        view.name = slot_name ? slot_name : "";

        if (!dbus_message_iter_next(&struct_iter) ||
            dbus_message_iter_get_arg_type(&struct_iter) != DBUS_TYPE_ARRAY) {
            dbus_message_unref(reply);
            return Result<std::vector<SlotStatusView>>::err("Invalid slot dictionary");
        }

        DBusMessageIter dict_iter;
        dbus_message_iter_recurse(&struct_iter, &dict_iter);

        while (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID) {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&dict_iter, &entry);

            const char* key = nullptr;
            dbus_message_iter_get_basic(&entry, &key);

            if (!dbus_message_iter_next(&entry)) {
                dbus_message_unref(reply);
                return Result<std::vector<SlotStatusView>>::err("Invalid dict entry");
            }

            DBusMessageIter variant;
            dbus_message_iter_recurse(&entry, &variant);

            std::string k = key ? key : "";
            int t = dbus_message_iter_get_arg_type(&variant);

            if (t == DBUS_TYPE_STRING) {
                const char* v = nullptr;
                dbus_message_iter_get_basic(&variant, &v);
                view.string_fields[k] = v ? v : "";
            } else if (t == DBUS_TYPE_BOOLEAN) {
                dbus_bool_t v = FALSE;
                dbus_message_iter_get_basic(&variant, &v);
                view.bool_fields[k] = (v == TRUE);
            } else if (t == DBUS_TYPE_UINT32) {
                dbus_uint32_t v = 0;
                dbus_message_iter_get_basic(&variant, &v);
                view.u32_fields[k] = static_cast<uint32_t>(v);
            } else if (t == DBUS_TYPE_UINT64) {
                dbus_uint64_t v = 0;
                dbus_message_iter_get_basic(&variant, &v);
                view.u64_fields[k] = static_cast<uint64_t>(v);
            } else if (t == DBUS_TYPE_INT32) {
                dbus_int32_t v = 0;
                dbus_message_iter_get_basic(&variant, &v);
                view.i32_fields[k] = static_cast<int32_t>(v);
            }

            dbus_message_iter_next(&dict_iter);
        }

        out.push_back(std::move(view));
        dbus_message_iter_next(&array_iter);
    }

    dbus_message_unref(reply);
    return Result<std::vector<SlotStatusView>>::ok(out);
}

} // namespace aegis
