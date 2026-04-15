#pragma once

#include "aegis/utils.h"
#include "aegis/progress_info.h"

#include <dbus/dbus.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace aegis {

struct SlotStatusView {
    std::string name;
    std::map<std::string, std::string> string_fields;
    std::map<std::string, bool> bool_fields;
    std::map<std::string, uint32_t> u32_fields;
    std::map<std::string, uint64_t> u64_fields;
    std::map<std::string, int32_t> i32_fields;
};

struct MarkResult {
    std::string slot_name;
    std::string message;
};

class AegisDbusClient {
public:
    AegisDbusClient();
    ~AegisDbusClient();

    AegisDbusClient(const AegisDbusClient&) = delete;
    AegisDbusClient& operator=(const AegisDbusClient&) = delete;

    Result<void> connect_system_bus();
    Result<void> subscribe_completed();

    Result<void> install_bundle(const std::string& bundle_path, bool ignore_compatible);
    Result<int> wait_for_completed();

    Result<std::string> get_property_string(const char* property_name);
    Result<ProgressInfo> get_progress();

    Result<std::string> get_primary();
    Result<std::vector<SlotStatusView>> get_slot_status();
    Result<MarkResult> mark(const std::string& state, const std::string& slot_identifier);

private:
    Result<DBusMessage*> call_method(DBusMessage* message);
    Result<DBusMessage*> call_properties_get(const char* property_name);

    DBusConnection* connection_ = nullptr;
};

} // namespace aegis
