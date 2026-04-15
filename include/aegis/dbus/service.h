#pragma once

#include "aegis/error.h"
#include "aegis/dbus/service_state.h"

#include <dbus/dbus.h>
#include <string>
#include <thread>
#include <vector>

namespace aegis {

struct InstallArgs;
struct Slot;
class Bundle;

class AegisService {
public:
    Result<void> run();
    void stop();

    static DBusHandlerResult handle_message(DBusConnection* connection,
                                            DBusMessage* message,
                                            void* user_data);

private:
    Result<void> connect_bus();
    Result<void> load_introspection_xml();
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

private:
    DBusConnection* connection_ = nullptr;
    std::thread install_thread_;
    ServiceState state_;
    std::string introspection_xml_;
};

Result<void> service_run();
void service_stop();

} // namespace aegis
