#include "aegis/service.h"
#include "aegis/bootchooser.h"
#include "aegis/bundle.h"
#include "aegis/context.h"
#include "aegis/install.h"
#include "aegis/mark.h"
#include "aegis/signature.h"
#include "aegis/utils.h"

#include <atomic>
#include <csignal>
#include <dbus/dbus.h>
#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

namespace aegis {

static std::atomic<bool> g_running{false};

static void signal_handler(int /*sig*/) {
    g_running = false;
}

namespace {

constexpr const char* kServiceName = "de.pengutronix.aegis";
constexpr const char* kObjectPath = "/";
constexpr const char* kInstallerInterface = "de.pengutronix.aegis.Installer";
constexpr const char* kPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr const char* kIntrospectableInterface = "org.freedesktop.DBus.Introspectable";

struct ProgressInfo {
    int percentage = 0;
    std::string message = "idle";
    int depth = 0;
};

class InstallerService {
public:
    Result<void> run();
    void stop();
    static DBusHandlerResult handle_message(DBusConnection* connection,
                                            DBusMessage* message,
                                            void* user_data);

private:
    Result<void> connect_bus();
    void maybe_run_autoinstall();
    void join_worker_if_needed();

    DBusHandlerResult dispatch(DBusMessage* message);

    DBusMessage* handle_introspect(DBusMessage* message);
    DBusMessage* handle_properties(DBusMessage* message);
    DBusMessage* handle_installer(DBusMessage* message);

    DBusMessage* handle_install(DBusMessage* message, bool allow_args);
    DBusMessage* handle_info(DBusMessage* message);
    DBusMessage* handle_inspect_bundle(DBusMessage* message);
    DBusMessage* handle_mark(DBusMessage* message);
    DBusMessage* handle_get_slot_status(DBusMessage* message);
    DBusMessage* handle_get_primary(DBusMessage* message);

    Result<void> start_install(const std::string& source, const InstallArgs& args);
    void finish_install(int result, std::string last_error);
    void update_progress(int percentage, std::string message, int depth);

    std::string variant() const;
    Slot* resolve_slot_identifier(const std::string& identifier) const;
    Slot* get_primary_slot() const;

    DBusMessage* error_reply(DBusMessage* message,
                             const char* name,
                             const std::string& text) const;
    void send_message(DBusMessage* message) const;
    void emit_completed(int result) const;
    void emit_properties_changed(const std::vector<std::string>& property_names) const;

    bool append_property_variant(DBusMessageIter* iter, const std::string& property) const;
    void append_slot_dict(DBusMessageIter* dict,
                          const Slot& slot,
                          const Slot* primary_slot) const;
    void append_inspect_dict(DBusMessageIter* dict, const Bundle& bundle) const;

    static void append_dict_entry_string(DBusMessageIter* dict,
                                         const char* key,
                                         const std::string& value);
    static void append_dict_entry_bool(DBusMessageIter* dict,
                                       const char* key,
                                       bool value);
    static void append_dict_entry_u32(DBusMessageIter* dict,
                                      const char* key,
                                      uint32_t value);
    static void append_dict_entry_u64(DBusMessageIter* dict,
                                      const char* key,
                                      uint64_t value);
    static void append_dict_entry_i32(DBusMessageIter* dict,
                                      const char* key,
                                      int32_t value);

    DBusConnection* connection_ = nullptr;
    mutable std::mutex mutex_;
    std::thread install_thread_;
    bool install_running_ = false;
    std::string operation_ = "idle";
    std::string last_error_;
    ProgressInfo progress_;
};

const char kIntrospectionXml[] =
    "<node>"
    "  <interface name='org.freedesktop.DBus.Introspectable'>"
    "    <method name='Introspect'>"
    "      <arg name='xml_data' type='s' direction='out'/>"
    "    </method>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface_name' type='s' direction='in'/>"
    "      <arg name='property_name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface_name' type='s' direction='in'/>"
    "      <arg name='props' type='a{sv}' direction='out'/>"
    "    </method>"
    "    <signal name='PropertiesChanged'>"
    "      <arg name='interface_name' type='s'/>"
    "      <arg name='changed_properties' type='a{sv}'/>"
    "      <arg name='invalidated_properties' type='as'/>"
    "    </signal>"
    "  </interface>"
    "  <interface name='de.pengutronix.aegis.Installer'>"
    "    <method name='InstallBundle'>"
    "      <arg name='source' type='s' direction='in'/>"
    "      <arg name='args' type='a{sv}' direction='in'/>"
    "    </method>"
    "    <method name='Install'>"
    "      <arg name='source' type='s' direction='in'/>"
    "    </method>"
    "    <method name='Info'>"
    "      <arg name='bundle' type='s' direction='in'/>"
    "      <arg name='compatible' type='s' direction='out'/>"
    "      <arg name='version' type='s' direction='out'/>"
    "    </method>"
    "    <method name='InspectBundle'>"
    "      <arg name='bundle' type='s' direction='in'/>"
    "      <arg name='args' type='a{sv}' direction='in'/>"
    "      <arg name='info' type='a{sv}' direction='out'/>"
    "    </method>"
    "    <method name='Mark'>"
    "      <arg name='state' type='s' direction='in'/>"
    "      <arg name='slot_identifier' type='s' direction='in'/>"
    "      <arg name='slot_name' type='s' direction='out'/>"
    "      <arg name='message' type='s' direction='out'/>"
    "    </method>"
    "    <method name='GetSlotStatus'>"
    "      <arg name='slot_status_array' type='a(sa{sv})' direction='out'/>"
    "    </method>"
    "    <method name='GetPrimary'>"
    "      <arg name='primary' type='s' direction='out'/>"
    "    </method>"
    "    <signal name='Completed'>"
    "      <arg name='result' type='i'/>"
    "    </signal>"
    "    <property name='Operation' type='s' access='read'/>"
    "    <property name='LastError' type='s' access='read'/>"
    "    <property name='Progress' type='(isi)' access='read'/>"
    "    <property name='Compatible' type='s' access='read'/>"
    "    <property name='Variant' type='s' access='read'/>"
    "    <property name='BootSlot' type='s' access='read'/>"
    "  </interface>"
    "</node>";

DBusObjectPathVTable kObjectPathVTable = {
    nullptr,
    &InstallerService::handle_message,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

DBusHandlerResult InstallerService::handle_message(DBusConnection* /*connection*/,
                                                   DBusMessage* message,
                                                   void* user_data) {
    auto* service = static_cast<InstallerService*>(user_data);
    return service->dispatch(message);
}

Result<void> InstallerService::connect_bus() {
    DBusError error;
    dbus_error_init(&error);

    DBusBusType bus_type = DBUS_BUS_SYSTEM;
    if (const char* bus_env = std::getenv("AEGIS_DBUS_BUS")) {
        if (std::string(bus_env) == "session")
            bus_type = DBUS_BUS_SESSION;
    }

    connection_ = dbus_bus_get(bus_type, &error);
    if (!connection_) {
        std::string msg = error.message ? error.message : "Unknown D-Bus error";
        dbus_error_free(&error);
        return Result<void>::err("Failed to connect to D-Bus bus: " + msg);
    }

    int request = dbus_bus_request_name(connection_, kServiceName,
                                        DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
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
    if (request != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        return Result<void>::err("Bus name de.pengutronix.aegis is already owned");

    if (!dbus_connection_register_object_path(connection_, kObjectPath,
                                              &kObjectPathVTable, this)) {
        return Result<void>::err("Failed to register D-Bus object path '/'");
    }

    return Result<void>::ok();
}

void InstallerService::join_worker_if_needed() {
    if (install_thread_.joinable() && !install_running_)
        install_thread_.join();
}

Result<void> InstallerService::start_install(const std::string& source,
                                             const InstallArgs& args) {
    if (source.empty())
        return Result<void>::err("Bundle source must not be empty");

    {
        std::lock_guard<std::mutex> lock(mutex_);
        join_worker_if_needed();
        if (install_running_)
            return Result<void>::err("Installation already in progress");

        install_running_ = true;
        operation_ = "installing";
        last_error_.clear();
        progress_ = {0, "Starting installation", 0};
    }

    emit_properties_changed({"Operation", "LastError", "Progress"});

    install_thread_ = std::thread([this, source, args]() mutable {
        InstallArgs install_args = args;
        install_args.name = source;
        install_args.progress = [this](int percentage, const std::string& message) {
            update_progress(percentage, message, 0);
        };
        install_args.status_notify = [this](const std::string& message) {
            int percentage = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                percentage = progress_.percentage;
            }
            update_progress(percentage, message, 0);
        };

        auto result = install_bundle(source, install_args);
        if (result) {
            update_progress(100, "Installation complete", 0);
            finish_install(0, {});
        } else {
            finish_install(1, result.error());
        }
    });

    return Result<void>::ok();
}

void InstallerService::finish_install(int result, std::string last_error) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        install_running_ = false;
        operation_ = "idle";
        last_error_ = std::move(last_error);
        if (result != 0 && progress_.message.empty())
            progress_.message = "Installation failed";
    }

    emit_properties_changed({"Operation", "LastError", "Progress"});
    emit_completed(result);
}

void InstallerService::update_progress(int percentage,
                                       std::string message,
                                       int depth) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_.percentage = percentage;
        progress_.message = std::move(message);
        progress_.depth = depth;
    }
    emit_properties_changed({"Progress"});
}

std::string InstallerService::variant() const {
    auto& cfg = Context::instance().config();
    if (!cfg.system_variant.empty()) return cfg.system_variant;
    if (!cfg.variant_name.empty()) return cfg.variant_name;
    if (!cfg.variant_file.empty()) return cfg.variant_file;
    return {};
}

Slot* InstallerService::resolve_slot_identifier(const std::string& identifier) const {
    auto& slots = Context::instance().config().slots;

    auto find_booted = [&]() -> Slot* {
        for (auto& [name, slot] : slots) {
            if (slot.is_booted) return &slot;
        }
        return nullptr;
    };

    if (identifier.empty() || identifier == "booted")
        return find_booted();

    if (identifier == "other") {
        auto* booted = find_booted();
        if (!booted) return nullptr;

        for (auto& [name, slot] : slots) {
            if (slot.name == booted->name) continue;
            if (slot.slot_class == booted->slot_class && slot.index != booted->index)
                return &slot;
        }

        for (auto& [name, slot] : slots) {
            if (!slot.is_booted) return &slot;
        }
        return nullptr;
    }

    if (auto it = slots.find(identifier); it != slots.end())
        return &it->second;

    for (auto& [name, slot] : slots) {
        if (slot.bootname == identifier)
            return &slot;
    }

    return nullptr;
}

Slot* InstallerService::get_primary_slot() const {
    auto& ctx = Context::instance();
    auto bootchooser = create_bootchooser(ctx.config());
    return bootchooser->get_primary(ctx.config().slots);
}

DBusMessage* InstallerService::error_reply(DBusMessage* message,
                                           const char* name,
                                           const std::string& text) const {
    return dbus_message_new_error(message, name, text.c_str());
}

void InstallerService::send_message(DBusMessage* message) const {
    if (!connection_ || !message) return;
    dbus_connection_send(connection_, message, nullptr);
    dbus_connection_flush(connection_);
}

void InstallerService::emit_completed(int result) const {
    DBusMessage* signal = dbus_message_new_signal(kObjectPath, kInstallerInterface,
                                                  "Completed");
    if (!signal) return;
    dbus_message_append_args(signal, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
    send_message(signal);
    dbus_message_unref(signal);
}

void InstallerService::append_dict_entry_string(DBusMessageIter* dict,
                                                const char* key,
                                                const std::string& value) {
    DBusMessageIter entry, variant;
    const char* c_key = key;
    const char* c_val = value.c_str();
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &c_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &c_val);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

void InstallerService::append_dict_entry_bool(DBusMessageIter* dict,
                                              const char* key,
                                              bool value) {
    DBusMessageIter entry, variant;
    const char* c_key = key;
    dbus_bool_t dbus_value = value ? TRUE : FALSE;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &c_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &dbus_value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

void InstallerService::append_dict_entry_u32(DBusMessageIter* dict,
                                             const char* key,
                                             uint32_t value) {
    DBusMessageIter entry, variant;
    const char* c_key = key;
    dbus_uint32_t dbus_value = value;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &c_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &dbus_value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

void InstallerService::append_dict_entry_u64(DBusMessageIter* dict,
                                             const char* key,
                                             uint64_t value) {
    DBusMessageIter entry, variant;
    const char* c_key = key;
    dbus_uint64_t dbus_value = value;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &c_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "t", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT64, &dbus_value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

void InstallerService::append_dict_entry_i32(DBusMessageIter* dict,
                                             const char* key,
                                             int32_t value) {
    DBusMessageIter entry, variant;
    const char* c_key = key;
    dbus_int32_t dbus_value = value;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &c_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "i", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_INT32, &dbus_value);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
}

bool InstallerService::append_property_variant(DBusMessageIter* iter,
                                               const std::string& property) const {
    auto& ctx = Context::instance();

    if (property == "Operation") {
        DBusMessageIter variant;
        std::string value;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            value = operation_;
        }
        const char* str = value.c_str();
        dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &str);
        dbus_message_iter_close_container(iter, &variant);
        return true;
    }

    if (property == "LastError") {
        DBusMessageIter variant;
        std::string value;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            value = last_error_;
        }
        const char* str = value.c_str();
        dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &str);
        dbus_message_iter_close_container(iter, &variant);
        return true;
    }

    if (property == "Compatible") {
        DBusMessageIter variant;
        const char* str = ctx.config().compatible.c_str();
        dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &str);
        dbus_message_iter_close_container(iter, &variant);
        return true;
    }

    if (property == "Variant") {
        DBusMessageIter variant_iter;
        auto value = variant();
        const char* str = value.c_str();
        dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &str);
        dbus_message_iter_close_container(iter, &variant_iter);
        return true;
    }

    if (property == "BootSlot") {
        DBusMessageIter variant_iter;
        const char* str = ctx.boot_slot().c_str();
        dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
        dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &str);
        dbus_message_iter_close_container(iter, &variant_iter);
        return true;
    }

    if (property == "Progress") {
        ProgressInfo progress;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            progress = progress_;
        }

        DBusMessageIter variant_iter, struct_iter;
        const char* message = progress.message.c_str();
        dbus_int32_t percentage = progress.percentage;
        dbus_int32_t depth = progress.depth;
        dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "(isi)", &variant_iter);
        dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_STRUCT, nullptr,
                                         &struct_iter);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &percentage);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &message);
        dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &depth);
        dbus_message_iter_close_container(&variant_iter, &struct_iter);
        dbus_message_iter_close_container(iter, &variant_iter);
        return true;
    }

    return false;
}

void InstallerService::emit_properties_changed(
    const std::vector<std::string>& property_names) const {
    if (!connection_) return;

    DBusMessage* signal = dbus_message_new_signal(kObjectPath, kPropertiesInterface,
                                                  "PropertiesChanged");
    if (!signal) return;

    DBusMessageIter iter, changed, invalidated;
    const char* iface = kInstallerInterface;
    dbus_message_iter_init_append(signal, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface);

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

void InstallerService::append_slot_dict(DBusMessageIter* dict,
                                        const Slot& slot,
                                        const Slot* primary_slot) const {
    append_dict_entry_string(dict, "class", slot.slot_class);
    append_dict_entry_i32(dict, "index", slot.index);
    append_dict_entry_string(dict, "device", slot.device);
    append_dict_entry_string(dict, "type", to_string(slot.type));
    append_dict_entry_string(dict, "bootname", slot.bootname);
    append_dict_entry_string(dict, "state", slot.status.status);
    append_dict_entry_bool(dict, "booted", slot.is_booted);
    append_dict_entry_bool(dict, "readonly", slot.readonly);
    append_dict_entry_bool(dict, "primary",
                           primary_slot && primary_slot->name == slot.name);

    if (!slot.parent_name.empty())
        append_dict_entry_string(dict, "parent", slot.parent_name);
    if (!slot.status.bundle_compatible.empty())
        append_dict_entry_string(dict, "bundle.compatible",
                                 slot.status.bundle_compatible);
    if (!slot.status.bundle_version.empty())
        append_dict_entry_string(dict, "bundle.version",
                                 slot.status.bundle_version);
    if (!slot.status.bundle_description.empty())
        append_dict_entry_string(dict, "bundle.description",
                                 slot.status.bundle_description);
    if (!slot.status.bundle_build.empty())
        append_dict_entry_string(dict, "bundle.build",
                                 slot.status.bundle_build);
    if (!slot.status.bundle_hash.empty())
        append_dict_entry_string(dict, "bundle.hash",
                                 slot.status.bundle_hash);
    if (!slot.status.checksum_sha256.empty())
        append_dict_entry_string(dict, "sha256", slot.status.checksum_sha256);
    if (slot.status.checksum_size > 0)
        append_dict_entry_u64(dict, "size", slot.status.checksum_size);
    if (!slot.status.installed_timestamp.empty())
        append_dict_entry_string(dict, "installed.timestamp",
                                 slot.status.installed_timestamp);
    append_dict_entry_u32(dict, "installed.count", slot.status.installed_count);
    if (!slot.status.activated_timestamp.empty())
        append_dict_entry_string(dict, "activated.timestamp",
                                 slot.status.activated_timestamp);
    append_dict_entry_u32(dict, "activated.count", slot.status.activated_count);
}

void InstallerService::append_inspect_dict(DBusMessageIter* dict,
                                           const Bundle& bundle) const {
    append_dict_entry_string(dict, "compatible", bundle.manifest.compatible);
    append_dict_entry_string(dict, "version", bundle.manifest.version);
    append_dict_entry_string(dict, "description", bundle.manifest.description);
    append_dict_entry_string(dict, "build", bundle.manifest.build);
    append_dict_entry_string(dict, "format", to_string(bundle.format));
    append_dict_entry_bool(dict, "verified", bundle.verified);
    append_dict_entry_u64(dict, "size", bundle.size);

    if (!bundle.manifest.verity_hash.empty())
        append_dict_entry_string(dict, "verity-hash", bundle.manifest.verity_hash);
    if (!bundle.manifest.verity_salt.empty())
        append_dict_entry_string(dict, "verity-salt", bundle.manifest.verity_salt);
    if (bundle.manifest.bundle_verity_size > 0)
        append_dict_entry_u64(dict, "verity-size",
                              bundle.manifest.bundle_verity_size);
}

DBusMessage* InstallerService::handle_introspect(DBusMessage* message) {
    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) return nullptr;
    const char* xml = kIntrospectionXml;
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* InstallerService::handle_properties(DBusMessage* message) {
    if (dbus_message_is_method_call(message, kPropertiesInterface, "Get")) {
        const char* interface_name = nullptr;
        const char* property_name = nullptr;
        if (!dbus_message_get_args(message, nullptr,
                                   DBUS_TYPE_STRING, &interface_name,
                                   DBUS_TYPE_STRING, &property_name,
                                   DBUS_TYPE_INVALID)) {
            return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                               "Expected interface and property name");
        }
        if (std::string(interface_name) != kInstallerInterface)
            return error_reply(message, DBUS_ERROR_UNKNOWN_INTERFACE,
                               "Unknown interface");

        DBusMessage* reply = dbus_message_new_method_return(message);
        if (!reply) return nullptr;
        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);
        if (!append_property_variant(&iter, property_name)) {
            dbus_message_unref(reply);
            return error_reply(message, DBUS_ERROR_UNKNOWN_PROPERTY,
                               "Unknown property: " + std::string(property_name));
        }
        return reply;
    }

    if (dbus_message_is_method_call(message, kPropertiesInterface, "GetAll")) {
        const char* interface_name = nullptr;
        if (!dbus_message_get_args(message, nullptr,
                                   DBUS_TYPE_STRING, &interface_name,
                                   DBUS_TYPE_INVALID)) {
            return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                               "Expected interface name");
        }
        if (std::string(interface_name) != kInstallerInterface)
            return error_reply(message, DBUS_ERROR_UNKNOWN_INTERFACE,
                               "Unknown interface");

        DBusMessage* reply = dbus_message_new_method_return(message);
        if (!reply) return nullptr;

        DBusMessageIter iter, array;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array);

        for (const auto& property :
             {"Operation", "LastError", "Progress", "Compatible", "Variant", "BootSlot"}) {
            DBusMessageIter entry;
            const char* key = property;
            dbus_message_iter_open_container(&array, DBUS_TYPE_DICT_ENTRY, nullptr,
                                             &entry);
            dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
            append_property_variant(&entry, property);
            dbus_message_iter_close_container(&array, &entry);
        }

        dbus_message_iter_close_container(&iter, &array);
        return reply;
    }

    return error_reply(message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown properties method");
}

DBusMessage* InstallerService::handle_install(DBusMessage* message, bool allow_args) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter))
        return error_reply(message, DBUS_ERROR_INVALID_ARGS, "Missing bundle source");

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
        return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                           "Bundle source must be a string");

    const char* source = nullptr;
    dbus_message_iter_get_basic(&iter, &source);

    InstallArgs args;

    if (allow_args) {
        if (!dbus_message_iter_next(&iter))
            return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                               "Missing install args dictionary");
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
            return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                               "Install args must be an a{sv} dictionary");

        DBusMessageIter dict_iter;
        dbus_message_iter_recurse(&iter, &dict_iter);
        while (dbus_message_iter_get_arg_type(&dict_iter) != DBUS_TYPE_INVALID) {
            DBusMessageIter entry, variant;
            dbus_message_iter_recurse(&dict_iter, &entry);

            const char* key = nullptr;
            dbus_message_iter_get_basic(&entry, &key);
            dbus_message_iter_next(&entry);
            dbus_message_iter_recurse(&entry, &variant);

            int type = dbus_message_iter_get_arg_type(&variant);
            std::string key_str = key ? key : "";

            if (key_str == "ignore-compatible" && type == DBUS_TYPE_BOOLEAN) {
                dbus_bool_t value = FALSE;
                dbus_message_iter_get_basic(&variant, &value);
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
    if (!result)
        return error_reply(message, "de.pengutronix.aegis.Error.Failed",
                           result.error());

    return dbus_message_new_method_return(message);
}

DBusMessage* InstallerService::handle_info(DBusMessage* message) {
    const char* bundle_path = nullptr;
    if (!dbus_message_get_args(message, nullptr,
                               DBUS_TYPE_STRING, &bundle_path,
                               DBUS_TYPE_INVALID)) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                           "Expected bundle path");
    }

    SigningParams params;
    params.keyring_path = Context::instance().keyring_path();

    auto info = bundle_info(bundle_path ? bundle_path : "", params, false);
    if (!info)
        return error_reply(message, "de.pengutronix.aegis.Error.Failed",
                           info.error());

    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) return nullptr;

    const char* compatible = info.value().manifest.compatible.c_str();
    const char* version = info.value().manifest.version.c_str();
    dbus_message_append_args(reply,
                             DBUS_TYPE_STRING, &compatible,
                             DBUS_TYPE_STRING, &version,
                             DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* InstallerService::handle_inspect_bundle(DBusMessage* message) {
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter))
        return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                           "Missing bundle path");

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
        return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                           "Bundle path must be a string");

    const char* bundle_path = nullptr;
    dbus_message_iter_get_basic(&iter, &bundle_path);

    SigningParams params;
    params.keyring_path = Context::instance().keyring_path();

    auto info = bundle_info(bundle_path ? bundle_path : "", params, false);
    if (!info)
        return error_reply(message, "de.pengutronix.aegis.Error.Failed",
                           info.error());

    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) return nullptr;

    DBusMessageIter reply_iter, dict;
    dbus_message_iter_init_append(reply, &reply_iter);
    dbus_message_iter_open_container(&reply_iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    append_inspect_dict(&dict, info.value());
    dbus_message_iter_close_container(&reply_iter, &dict);
    return reply;
}

DBusMessage* InstallerService::handle_mark(DBusMessage* message) {
    const char* state = nullptr;
    const char* slot_identifier = nullptr;
    if (!dbus_message_get_args(message, nullptr,
                               DBUS_TYPE_STRING, &state,
                               DBUS_TYPE_STRING, &slot_identifier,
                               DBUS_TYPE_INVALID)) {
        return error_reply(message, DBUS_ERROR_INVALID_ARGS,
                           "Expected state and slot identifier");
    }

    auto* slot = resolve_slot_identifier(slot_identifier ? slot_identifier : "");
    if (!slot)
        return error_reply(message, "de.pengutronix.aegis.Error.Failed",
                           "Unable to resolve slot identifier");

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

    if (!result)
        return error_reply(message, "de.pengutronix.aegis.Error.Failed",
                           result.error());

    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) return nullptr;

    const char* slot_name = slot->name.c_str();
    const char* msg = message_text.c_str();
    dbus_message_append_args(reply,
                             DBUS_TYPE_STRING, &slot_name,
                             DBUS_TYPE_STRING, &msg,
                             DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* InstallerService::handle_get_slot_status(DBusMessage* message) {
    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) return nullptr;

    auto* primary_slot = get_primary_slot();

    DBusMessageIter iter, array;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "(sa{sv})", &array);

    for (const auto& [name, slot] : Context::instance().config().slots) {
        DBusMessageIter slot_struct, slot_dict;
        const char* slot_name = name.c_str();
        dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, nullptr,
                                         &slot_struct);
        dbus_message_iter_append_basic(&slot_struct, DBUS_TYPE_STRING, &slot_name);
        dbus_message_iter_open_container(&slot_struct, DBUS_TYPE_ARRAY, "{sv}",
                                         &slot_dict);
        append_slot_dict(&slot_dict, slot, primary_slot);
        dbus_message_iter_close_container(&slot_struct, &slot_dict);
        dbus_message_iter_close_container(&array, &slot_struct);
    }

    dbus_message_iter_close_container(&iter, &array);
    return reply;
}

DBusMessage* InstallerService::handle_get_primary(DBusMessage* message) {
    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) return nullptr;

    auto* primary = get_primary_slot();
    std::string value = primary ? primary->name : "";
    const char* primary_name = value.c_str();
    dbus_message_append_args(reply,
                             DBUS_TYPE_STRING, &primary_name,
                             DBUS_TYPE_INVALID);
    return reply;
}

DBusMessage* InstallerService::handle_installer(DBusMessage* message) {
    if (dbus_message_is_method_call(message, kInstallerInterface, "InstallBundle"))
        return handle_install(message, true);
    if (dbus_message_is_method_call(message, kInstallerInterface, "Install"))
        return handle_install(message, false);
    if (dbus_message_is_method_call(message, kInstallerInterface, "Info"))
        return handle_info(message);
    if (dbus_message_is_method_call(message, kInstallerInterface, "InspectBundle"))
        return handle_inspect_bundle(message);
    if (dbus_message_is_method_call(message, kInstallerInterface, "Mark"))
        return handle_mark(message);
    if (dbus_message_is_method_call(message, kInstallerInterface, "GetSlotStatus"))
        return handle_get_slot_status(message);
    if (dbus_message_is_method_call(message, kInstallerInterface, "GetPrimary"))
        return handle_get_primary(message);

    return error_reply(message, DBUS_ERROR_UNKNOWN_METHOD, "Unknown installer method");
}

DBusHandlerResult InstallerService::dispatch(DBusMessage* message) {
    DBusMessage* reply = nullptr;

    try {
        if (dbus_message_is_method_call(message, kIntrospectableInterface, "Introspect")) {
            reply = handle_introspect(message);
        } else if (dbus_message_has_interface(message, kPropertiesInterface)) {
            reply = handle_properties(message);
        } else if (dbus_message_has_interface(message, kInstallerInterface)) {
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

void InstallerService::maybe_run_autoinstall() {
    auto& ctx = Context::instance();
    if (ctx.config().autoinstall_path.empty() ||
        !path_exists(ctx.config().autoinstall_path)) {
        return;
    }

    LOG_INFO("Auto-install bundle found: %s", ctx.config().autoinstall_path.c_str());
    InstallArgs args;
    args.name = ctx.config().autoinstall_path;
    auto result = start_install(ctx.config().autoinstall_path, args);
    if (!result)
        LOG_ERROR("Auto-install failed to start: %s", result.error().c_str());
}

Result<void> InstallerService::run() {
    auto connect_result = connect_bus();
    if (!connect_result)
        return connect_result;

    maybe_run_autoinstall();

    while (g_running) {
        dbus_connection_read_write_dispatch(connection_, 100);
    }

    join_worker_if_needed();
    if (install_thread_.joinable())
        install_thread_.join();

    if (connection_) {
        dbus_connection_unregister_object_path(connection_, kObjectPath);
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }

    return Result<void>::ok();
}

void InstallerService::stop() {
    g_running = false;
}

} // namespace

Result<void> service_run() {
    auto& ctx = Context::instance();
    if (!ctx.is_initialized())
        return Result<void>::err("Context not initialized");

    dbus_threads_init_default();

    LOG_INFO("Starting Aegis service (compatible=%s, bootloader=%s)",
             ctx.config().compatible.c_str(),
             to_string(ctx.config().bootloader));

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    g_running = true;

    InstallerService service;
    auto result = service.run();
    if (!result)
        return result;

    LOG_INFO("Aegis service stopped");
    return Result<void>::ok();
}

void service_stop() {
    g_running = false;
}

} // namespace aegis
