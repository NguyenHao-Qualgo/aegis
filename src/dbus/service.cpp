#include "aegis/dbus/service.h"

#include "aegis/bootchooser.h"
#include "aegis/bundle.h"
#include "aegis/context.h"
#include "aegis/dbus/interface.h"
#include "aegis/dbus/message_builder.h"
#include "aegis/install.h"
#include "aegis/mark.h"
#include "aegis/signature.h"
#include "aegis/utils.h"
#include "config.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <utility>

namespace aegis {

static std::atomic<bool> g_running{false};

static void signal_handler(int /*sig*/) {
    g_running = false;
}

namespace {
DBusObjectPathVTable kObjectPathVTable = {
    nullptr, &AegisService::handle_message, nullptr, nullptr, nullptr, nullptr};

} // namespace

DBusHandlerResult AegisService::handle_message(DBusConnection* /*connection*/, DBusMessage* message,
                                               void* user_data) {
    auto* service = static_cast<AegisService*>(user_data);
    return service->dispatch(message);
}

Result<void> AegisService::load_introspection_xml() {
    const char* configured_path = std::getenv("AEGIS_DBUS_INTROSPECTION_XML");
    std::string path = (configured_path != nullptr && *configured_path != '\0')
                           ? configured_path
                           : AEGIS_DBUS_INTROSPECTION_XML_PATH;

    try {
        introspection_xml_ = read_text_file(path);
    } catch (const std::exception& e) {
        return Result<void>::err("Failed to load D-Bus introspection XML from '" + path +
                                 "': " + e.what());
    }

    if (introspection_xml_.empty()) {
        return Result<void>::err("Failed to load D-Bus introspection XML from '" + path +
                                 "': file is empty");
    }

    return Result<void>::ok();
}

Result<void> AegisService::connect_bus() {
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

    int request =
        dbus_bus_request_name(connection_, dbus::kServiceName, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
    if (dbus_error_is_set(&error)) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        if (bus_type == DBUS_BUS_SYSTEM) {
            return Result<void>::err(
                "Failed to request bus name: " + msg +
                ". Install packaging/dbus-1/system.d/de.pengutronix.aegis.conf "
                "to your system D-Bus policy directory, then restart the Aegis "
                "service or start a fresh boot/session. For development, set "
                "AEGIS_DBUS_BUS=session.");
        }
        return Result<void>::err("Failed to request bus name: " + msg);
    }

    if (request != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        return Result<void>::err("Bus name de.pengutronix.aegis is already owned");
    }

    if (!dbus_connection_register_object_path(connection_, dbus::kObjectPath, &kObjectPathVTable,
                                              this)) {
        return Result<void>::err("Failed to register D-Bus object path '/'");
    }

    return Result<void>::ok();
}

void AegisService::join_worker_if_needed() {
    if (install_thread_.joinable() && !state_.install_running()) {
        install_thread_.join();
    }
}

Result<void> AegisService::start_install(const std::string& source, const InstallArgs& args) {
    if (source.empty()) {
        return Result<void>::err("Bundle source must not be empty");
    }

    join_worker_if_needed();
    if (state_.install_running()) {
        return Result<void>::err("Installation already in progress");
    }

    state_.start_installing();
    emit_properties_changed({"Operation", "LastError", "Progress"});

    install_thread_ = std::thread([this, source, args]() mutable {
        InstallArgs install_args = args;
        install_args.name = source;
        install_args.progress = [this](int percentage, const std::string& message) {
            state_.update_progress(percentage, message, 0);
            emit_properties_changed({"Progress"});
        };
        install_args.status_notify = [this](const std::string& message) {
            auto p = state_.progress();
            state_.update_progress(p.percentage, message, 0);
            emit_properties_changed({"Progress"});
        };

        auto result = install_bundle(source, install_args);
        if (result) {
            state_.update_progress(100, "Installation complete", 0);
            emit_properties_changed({"Progress"});
            state_.finish_install(0, {});
            emit_properties_changed({"Operation", "LastError", "Progress"});
            emit_completed(0);
        } else {
            state_.finish_install(1, result.error());
            emit_properties_changed({"Operation", "LastError", "Progress"});
            emit_completed(1);
        }
    });

    return Result<void>::ok();
}

std::string AegisService::variant() const {
    auto& cfg = Context::instance().config();
    if (!cfg.system_variant.empty()) {
        return cfg.system_variant;
    }
    if (!cfg.variant_name.empty()) {
        return cfg.variant_name;
    }
    if (!cfg.variant_file.empty()) {
        return cfg.variant_file;
    }
    return {};
}

Slot* AegisService::resolve_slot_identifier(const std::string& identifier) const {
    auto& slots = Context::instance().config().slots;

    auto find_booted = [&]() -> Slot* {
        for (auto& [_, slot] : slots) {
            if (slot.is_booted) {
                return &slot;
            }
        }
        return nullptr;
    };

    if (identifier.empty() || identifier == "booted") {
        return find_booted();
    }

    if (identifier == "other") {
        auto* booted = find_booted();
        if (booted == nullptr) {
            return nullptr;
        }

        for (auto& [_, slot] : slots) {
            if (slot.name == booted->name) {
                continue;
            }
            if (slot.slot_class == booted->slot_class && slot.index != booted->index) {
                return &slot;
            }
        }

        for (auto& [_, slot] : slots) {
            if (!slot.is_booted) {
                return &slot;
            }
        }
        return nullptr;
    }

    if (auto it = slots.find(identifier); it != slots.end()) {
        return &it->second;
    }

    for (auto& [_, slot] : slots) {
        if (slot.bootname == identifier) {
            return &slot;
        }
    }

    return nullptr;
}

Slot* AegisService::get_primary_slot() const {
    auto& ctx = Context::instance();
    auto bootchooser = create_bootchooser(ctx.config());
    return bootchooser->get_primary(ctx.config().slots);
}

DBusMessage* AegisService::error_reply(DBusMessage* message, const char* name,
                                       const std::string& text) const {
    return DbusMessageBuilder::make_error_reply(message, name, text);
}

void AegisService::send_message(DBusMessage* message) const {
    if (connection_ == nullptr || message == nullptr) {
        return;
    }
    dbus_connection_send(connection_, message, nullptr);
    dbus_connection_flush(connection_);
}

void AegisService::emit_completed(int result) const {
    DBusMessage* signal =
        dbus_message_new_signal(dbus::kObjectPath, dbus::kInstallerInterface, "Completed");
    if (signal == nullptr) {
        return;
    }
    dbus_message_append_args(signal, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
    send_message(signal);
    dbus_message_unref(signal);
}

bool AegisService::append_property_variant(DBusMessageIter* iter,
                                           const std::string& property) const {
    auto& ctx = Context::instance();

    if (property == "Operation") {
        return DbusMessageBuilder::append_string_property(iter, state_.operation());
    }
    if (property == "LastError") {
        return DbusMessageBuilder::append_string_property(iter, state_.last_error());
    }
    if (property == "Compatible") {
        return DbusMessageBuilder::append_string_property(iter, ctx.config().compatible);
    }
    if (property == "Variant") {
        return DbusMessageBuilder::append_string_property(iter, variant());
    }
    if (property == "BootSlot") {
        return DbusMessageBuilder::append_string_property(iter, ctx.boot_slot());
    }
    if (property == "ServiceVersion") {
        return DbusMessageBuilder::append_string_property(iter, AEGIS_BUILD_VERSION);
    }
    if (property == "Bootloader") {
        return DbusMessageBuilder::append_string_property(iter, to_string(ctx.config().bootloader));
    }
    if (property == "Progress") {
        return DbusMessageBuilder::append_progress_property(iter, state_.progress());
    }

    return false;
}

void AegisService::emit_properties_changed(const std::vector<std::string>& property_names) const {
    if (connection_ == nullptr) {
        return;
    }

    DBusMessage* signal =
        dbus_message_new_signal(dbus::kObjectPath, dbus::kPropertiesInterface, "PropertiesChanged");
    if (signal == nullptr) {
        return;
    }

    DBusMessageIter iter;
    DBusMessageIter changed;
    DBusMessageIter invalidated;
    const char* iface = dbus::kInstallerInterface;
    dbus_message_iter_init_append(signal, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, static_cast<const void*>(&iface));

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &changed);
    for (const auto& property : property_names) {
        DBusMessageIter entry;
        const char* key = property.c_str();
        dbus_message_iter_open_container(&changed, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        if (!append_property_variant(&entry, property)) {
            dbus_message_iter_abandon_container(&changed, &entry);
            continue;
        }
        dbus_message_iter_close_container(&changed, &entry);
    }
    dbus_message_iter_close_container(&iter, &changed);

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &invalidated);
    dbus_message_iter_close_container(&iter, &invalidated);

    send_message(signal);
    dbus_message_unref(signal);
}

DBusMessage* AegisService::handle_introspect(DBusMessage* message) {
    return DbusMessageBuilder::make_introspect_reply(message, introspection_xml_.c_str());
}

DBusMessage* AegisService::handle_properties(DBusMessage* message) {
    if (dbus_message_is_method_call(message, dbus::kPropertiesInterface, "Get")) {
        const char* interface_name = nullptr;
        const char* property_name = nullptr;
        if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &interface_name,
                                   DBUS_TYPE_STRING, &property_name, DBUS_TYPE_INVALID)) {
            return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                               "Expected interface and property name");
        }

        if (std::string(interface_name) != dbus::kInstallerInterface) {
            return error_reply(message, DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
        }

        DBusMessage* reply = dbus_message_new_method_return(message);
        if (reply == nullptr) {
            return nullptr;
        }

        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);
        if (!append_property_variant(&iter, property_name)) {
            dbus_message_unref(reply);
            return error_reply(message, DBUS_ERROR_UNKNOWN_PROPERTY,
                               "Unknown property: " + std::string(property_name));
        }
        return reply;
    }

    if (dbus_message_is_method_call(message, dbus::kPropertiesInterface, "GetAll")) {
        const char* interface_name = nullptr;
        if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &interface_name,
                                   DBUS_TYPE_INVALID)) {
            return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Expected interface name");
        }

        if (std::string(interface_name) != dbus::kInstallerInterface) {
            return error_reply(message, DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
        }

        DBusMessage* reply = dbus_message_new_method_return(message);
        if (reply == nullptr) {
            return nullptr;
        }

        DBusMessageIter iter;
        DBusMessageIter array;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array);

        for (const auto* property : dbus::kInstallerProperties) {
            DBusMessageIter entry;
            const char* key = property;
            dbus_message_iter_open_container(&array, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, static_cast<const void*>(&key));
            append_property_variant(&entry, property);
            dbus_message_iter_close_container(&array, &entry);
        }

        dbus_message_iter_close_container(&iter, &array);
        return reply;
    }

    return error_reply(message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown properties method");
}

DBusMessage* AegisService::handle_install(DBusMessage* message, bool allow_args) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter)) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Missing bundle source");
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Bundle source must be a string");
    }

    const char* source = nullptr;
    dbus_message_iter_get_basic(&iter, static_cast<void*>(&source));

    InstallArgs args;

    if (allow_args) {
        if (!dbus_message_iter_next(&iter)) {
            return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Missing install args dictionary");
        }
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
            return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                               "Install args must be an a{sv} dictionary");
        }

        DBusMessageIter dict_iter;
        dbus_message_iter_recurse(&iter, &dict_iter);
        while (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID) {
            DBusMessageIter entry;
            DBusMessageIter variant;
            dbus_message_iter_recurse(&dict_iter, &entry);

            const char* key = nullptr;
            dbus_message_iter_get_basic(&entry, static_cast<void*>(&key));
            dbus_message_iter_next(&entry);
            dbus_message_iter_recurse(&entry, &variant);

            int type = dbus_message_iter_get_arg_type(&variant);
            std::string key_str = key ? key : "";

            if (key_str == "ignore-compatible" && type == DBUS_TYPE_BOOLEAN) {
                dbus_bool_t value = FALSE;
                dbus_message_iter_get_basic(&variant, static_cast<void*>(&value));
                args.ignore_compatible = value;
            } else if (key_str == "ignore-version-limit" && type == DBUS_TYPE_BOOLEAN) {
                dbus_bool_t value = FALSE;
                dbus_message_iter_get_basic(&variant, &value);
                args.ignore_version_limit = value;
            } else if (key_str == "transaction-id" && type == DBUS_TYPE_STRING) {
                const char* value = nullptr;
                dbus_message_iter_get_basic(&variant, &value);
                args.transaction_id = value ? value : "";
            }

            dbus_message_iter_next(&dict_iter);
        }
    }

    auto result = start_install(source ? source : "", args);
    if (!result) {
        return error_reply(message, "de.pengutronix.aegis.Error.Failed", result.error());
    }

    return dbus_message_new_method_return(message);
}

DBusMessage* AegisService::handle_info(DBusMessage* message) {
    const char* bundle_path = nullptr;
    if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &bundle_path,
                               DBUS_TYPE_INVALID)) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Expected bundle path");
    }

    SigningParams params;
    params.keyring_path = Context::instance().keyring_path();

    auto info = bundle_info(bundle_path ? bundle_path : "", params, false);
    if (!info) {
        return error_reply(message, "de.pengutronix.aegis.Error.Failed", info.error());
    }

    DBusMessage* reply = dbus_message_new_method_return(message);
    if (reply == nullptr) {
        return nullptr;
    }

    const char* compatible = info.value().manifest.compatible.c_str();
    const char* version = info.value().manifest.version.c_str();
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &compatible, DBUS_TYPE_STRING, &version,
                             DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* AegisService::handle_inspect_bundle(DBusMessage* message) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter)) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Missing bundle path");
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Bundle path must be a string");
    }

    const char* bundle_path = nullptr;
    dbus_message_iter_get_basic(&iter, static_cast<void*>(&bundle_path));

    SigningParams params;
    params.keyring_path = Context::instance().keyring_path();

    auto info = bundle_info(bundle_path ? bundle_path : "", params, false);
    if (!info) {
        return error_reply(message, "de.pengutronix.aegis.Error.Failed", info.error());
    }

    DBusMessage* reply = dbus_message_new_method_return(message);
    if (reply == nullptr) {
        return nullptr;
    }

    DBusMessageIter reply_iter;
    DBusMessageIter dict;
    dbus_message_iter_init_append(reply, &reply_iter);
    dbus_message_iter_open_container(&reply_iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    DbusMessageBuilder::append_inspect_dict(&dict, info.value());
    dbus_message_iter_close_container(&reply_iter, &dict);
    return reply;
}

DBusMessage* AegisService::handle_mark(DBusMessage* message) {
    const char* state = nullptr;
    const char* slot_identifier = nullptr;
    if (!dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &state, DBUS_TYPE_STRING,
                               &slot_identifier, DBUS_TYPE_INVALID)) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Expected state and slot identifier");
    }

    auto* slot = resolve_slot_identifier(slot_identifier ? slot_identifier : "");
    if (!slot) {
        return error_reply(message, "de.pengutronix.aegis.Error.Failed",
                           "Unable to resolve slot identifier");
    }

    std::string message_text;
    Result<void> result = Result<void>::err("Unsupported mark state");
    std::string state_str = state ? state : "";

    if (state_str == "good") {
        result = mark_good(slot_identifier ? slot_identifier : "");
        message_text = "marked slot " + slot->name + " good";
    } else if (state_str == "bad") {
        result = mark_bad(slot_identifier ? slot_identifier : "");
        message_text = "marked slot " + slot->name + " bad";
    } else if (state_str == "active") {
        result = mark_active(slot_identifier ? slot_identifier : "");
        message_text = "activated slot " + slot->name;
    }

    if (!result) {
        return error_reply(message, "de.pengutronix.aegis.Error.Failed", result.error());
    }

    DBusMessage* reply = dbus_message_new_method_return(message);
    if (reply == nullptr) {
        return nullptr;
    }

    const char* slot_name = slot->name.c_str();
    const char* msg = message_text.c_str();
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &slot_name, DBUS_TYPE_STRING, &msg,
                             DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* AegisService::handle_get_slot_status(DBusMessage* message) {
    DBusMessage* reply = dbus_message_new_method_return(message);
    if (reply == nullptr) {
        return nullptr;
    }

    auto* primary_slot = get_primary_slot();

    DBusMessageIter iter;
    DBusMessageIter array;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(sa{sv})", &array);

    for (const auto& [name, slot] : Context::instance().config().slots) {
        DBusMessageIter slot_struct;
        DBusMessageIter slot_dict;
        const char* slot_name = name.c_str();
        dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, nullptr, &slot_struct);
        dbus_message_iter_append_basic(&slot_struct, DBUS_TYPE_STRING,
                                       static_cast<const void*>(&slot_name));
        dbus_message_iter_open_container(&slot_struct, DBUS_TYPE_ARRAY, "{sv}", &slot_dict);
        DbusMessageBuilder::append_slot_dict(&slot_dict, slot, primary_slot);
        dbus_message_iter_close_container(&slot_struct, &slot_dict);
        dbus_message_iter_close_container(&array, &slot_struct);
    }

    dbus_message_iter_close_container(&iter, &array);
    return reply;
}

DBusMessage* AegisService::handle_get_primary(DBusMessage* message) {
    DBusMessage* reply = dbus_message_new_method_return(message);
    if (reply == nullptr) {
        return nullptr;
    }

    auto* primary = get_primary_slot();
    std::string value = primary ? primary->name : "";
    const char* primary_name = value.c_str();
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &primary_name, DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* AegisService::handle_installer(DBusMessage* message) {
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "InstallBundle")) {
        return handle_install(message, true);
    }
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "Install")) {
        return handle_install(message, false);
    }
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "Info")) {
        return handle_info(message);
    }
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "InspectBundle")) {
        return handle_inspect_bundle(message);
    }
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "Mark")) {
        return handle_mark(message);
    }
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "GetSlotStatus")) {
        return handle_get_slot_status(message);
    }
    if (dbus_message_is_method_call(message, dbus::kInstallerInterface, "GetPrimary")) {
        return handle_get_primary(message);
    }

    return error_reply(message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown installer method");
}

DBusHandlerResult AegisService::dispatch(DBusMessage* message) {
    DBusMessage* reply = nullptr;

    try {
        if (dbus_message_is_method_call(message, dbus::kIntrospectableInterface, "Introspect")) {
            reply = handle_introspect(message);
        } else if (dbus_message_has_interface(message, dbus::kPropertiesInterface)) {
            reply = handle_properties(message);
        } else if (dbus_message_has_interface(message, dbus::kInstallerInterface)) {
            reply = handle_installer(message);
        } else {
            reply = error_reply(message, DBUS_ERROR_UNKNOWN_METHOD, "Unsupported method");
        }
    } catch (const std::exception& e) {
        reply = error_reply(message, "de.pengutronix.aegis.Error.Failed", e.what());
    }

    if (reply) {
        send_message(reply);
        dbus_message_unref(reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

void AegisService::maybe_run_autoinstall() {
    auto& ctx = Context::instance();
    if (ctx.config().autoinstall_path.empty() || !path_exists(ctx.config().autoinstall_path)) {
        return;
    }

    LOG_INFO("Auto-install bundle found: %s", ctx.config().autoinstall_path.c_str());

    InstallArgs args;
    args.name = ctx.config().autoinstall_path;

    auto result = start_install(ctx.config().autoinstall_path, args);
    if (!result) {
        LOG_ERROR("Auto-install failed to start: %s", result.error().c_str());
    }
}

Result<void> AegisService::run() {
    auto introspection_result = load_introspection_xml();
    if (!introspection_result) {
        return introspection_result;
    }

    auto connect_result = connect_bus();
    if (!connect_result) {
        return connect_result;
    }

    maybe_run_autoinstall();

    while (g_running) {
        dbus_connection_read_write_dispatch(connection_, 100);
    }

    join_worker_if_needed();
    if (install_thread_.joinable()) {
        install_thread_.join();
    }

    if (connection_) {
        dbus_connection_unregister_object_path(connection_, dbus::kObjectPath);
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }

    return Result<void>::ok();
}

void AegisService::stop() {
    g_running = false;
}

Result<void> service_run() {
    auto& ctx = Context::instance();
    if (!ctx.is_initialized()) {
        return Result<void>::err("Context not initialized");
    }

    dbus_threads_init_default();

    LOG_INFO("Starting Aegis service (compatible=%s, bootloader=%s)",
             ctx.config().compatible.c_str(), to_string(ctx.config().bootloader));

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    g_running = true;

    AegisService service;
    auto result = service.run();
    if (!result) {
        return result;
    }

    LOG_INFO("Aegis service stopped");
    return Result<void>::ok();
}

void service_stop() {
    g_running = false;
}

} // namespace aegis
