#pragma once

#include "aegis/bundle.h"
#include "aegis/dbus/service_state.h"

#include <dbus/dbus.h>
#include <string>
#include <vector>

namespace aegis {

struct Slot;

class DbusMessageBuilder {
  public:
    static DBusMessage* make_error_reply(DBusMessage* message, const char* name,
                                         const std::string& text);

    static void append_dict_entry_string(DBusMessageIter* dict, const char* key,
                                         const std::string& value);
    static void append_dict_entry_bool(DBusMessageIter* dict, const char* key, bool value);
    static void append_dict_entry_u32(DBusMessageIter* dict, const char* key, uint32_t value);
    static void append_dict_entry_u64(DBusMessageIter* dict, const char* key, uint64_t value);
    static void append_dict_entry_i32(DBusMessageIter* dict, const char* key, int32_t value);

    static void append_slot_dict(DBusMessageIter* dict, const Slot& slot, const Slot* primary_slot);

    static void append_inspect_dict(DBusMessageIter* dict, const Bundle& bundle);

    static bool append_string_property(DBusMessageIter* iter, const std::string& value);
    static bool append_progress_property(DBusMessageIter* iter, const ProgressInfo& progress);

    static DBusMessage* make_introspect_reply(DBusMessage* message, const char* xml);
};

} // namespace aegis
