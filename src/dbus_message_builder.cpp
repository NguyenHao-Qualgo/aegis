#include "aegis/dbus_message_builder.h"
#include "aegis/context.h"
#include "aegis/utils.h"

namespace aegis {

DBusMessage* DbusMessageBuilder::make_error_reply(DBusMessage* message,
                                                  const char* name,
                                                  const std::string& text) {
    return dbus_message_new_error(message, name, text.c_str());
}

void DbusMessageBuilder::append_dict_entry_string(DBusMessageIter* dict,
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

void DbusMessageBuilder::append_dict_entry_bool(DBusMessageIter* dict,
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

void DbusMessageBuilder::append_dict_entry_u32(DBusMessageIter* dict,
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

void DbusMessageBuilder::append_dict_entry_u64(DBusMessageIter* dict,
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

void DbusMessageBuilder::append_dict_entry_i32(DBusMessageIter* dict,
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

bool DbusMessageBuilder::append_string_property(DBusMessageIter* iter, const std::string& value) {
    DBusMessageIter variant;
    const char* str = value.c_str();
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &str);
    dbus_message_iter_close_container(iter, &variant);
    return true;
}

bool DbusMessageBuilder::append_progress_property(DBusMessageIter* iter,
                                                  const ProgressInfo& progress) {
    DBusMessageIter variant_iter, struct_iter;
    const char* message = progress.message.c_str();
    dbus_int32_t percentage = progress.percentage;
    dbus_int32_t depth = progress.depth;

    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "(isi)", &variant_iter);
    dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_STRUCT, nullptr, &struct_iter);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &percentage);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &message);
    dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_INT32, &depth);
    dbus_message_iter_close_container(&variant_iter, &struct_iter);
    dbus_message_iter_close_container(iter, &variant_iter);
    return true;
}

void DbusMessageBuilder::append_slot_dict(DBusMessageIter* dict,
                                          const Slot& slot,
                                          const Slot* primary_slot) {
    append_dict_entry_string(dict, "class", slot.slot_class);
    append_dict_entry_i32(dict, "index", slot.index);
    append_dict_entry_string(dict, "device", slot.device);
    append_dict_entry_string(dict, "type", to_string(slot.type));
    append_dict_entry_string(dict, "bootname", slot.bootname);
    append_dict_entry_string(dict, "state", slot.status.status);
    append_dict_entry_bool(dict, "booted", slot.is_booted);
    append_dict_entry_bool(dict, "readonly", slot.readonly);
    append_dict_entry_bool(dict, "primary", primary_slot && primary_slot->name == slot.name);

    if (!slot.parent_name.empty())
        append_dict_entry_string(dict, "parent", slot.parent_name);
    if (!slot.status.bundle_compatible.empty())
        append_dict_entry_string(dict, "bundle.compatible", slot.status.bundle_compatible);
    if (!slot.status.bundle_version.empty())
        append_dict_entry_string(dict, "bundle.version", slot.status.bundle_version);
    if (!slot.status.bundle_description.empty())
        append_dict_entry_string(dict, "bundle.description", slot.status.bundle_description);
    if (!slot.status.bundle_build.empty())
        append_dict_entry_string(dict, "bundle.build", slot.status.bundle_build);
    if (!slot.status.bundle_hash.empty())
        append_dict_entry_string(dict, "bundle.hash", slot.status.bundle_hash);
    if (!slot.status.checksum_sha256.empty())
        append_dict_entry_string(dict, "sha256", slot.status.checksum_sha256);
    if (slot.status.checksum_size > 0)
        append_dict_entry_u64(dict, "size", slot.status.checksum_size);
    if (!slot.status.installed_timestamp.empty())
        append_dict_entry_string(dict, "installed.timestamp", slot.status.installed_timestamp);
    append_dict_entry_u32(dict, "installed.count", slot.status.installed_count);
    if (!slot.status.activated_timestamp.empty())
        append_dict_entry_string(dict, "activated.timestamp", slot.status.activated_timestamp);
    append_dict_entry_u32(dict, "activated.count", slot.status.activated_count);
}

void DbusMessageBuilder::append_inspect_dict(DBusMessageIter* dict,
                                             const Bundle& bundle) {
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
        append_dict_entry_u64(dict, "verity-size", bundle.manifest.bundle_verity_size);
}

DBusMessage* DbusMessageBuilder::make_introspect_reply(DBusMessage* message, const char* xml) {
    DBusMessage* reply = dbus_message_new_method_return(message);
    if (!reply) {
        return nullptr;
    }
    dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
    return reply;
}

} // namespace aegis