#include "rauc/mark.h"
#include "rauc/bootchooser.h"
#include "rauc/context.h"
#include "rauc/status_file.h"
#include "rauc/utils.h"

namespace rauc {

static Slot* resolve_slot(const std::string& identifier) {
    auto& ctx = Context::instance();
    auto& slots = ctx.config().slots;

    if (identifier.empty()) {
        // Default: the booted slot
        for (auto& [name, slot] : slots) {
            if (slot.is_booted) return &slot;
        }
        throw SlotError("Cannot determine booted slot");
    }

    // Try by name
    auto it = slots.find(identifier);
    if (it != slots.end()) return &it->second;

    // Try by bootname
    for (auto& [name, slot] : slots) {
        if (slot.bootname == identifier) return &slot;
    }

    throw SlotError("Slot not found: " + identifier);
}

Result<void> mark_good(const std::string& slot_identifier) {
    auto& ctx = Context::instance();
    auto* slot = resolve_slot(slot_identifier);

    auto bootchooser = create_bootchooser(ctx.config());
    auto res = bootchooser->set_state(*slot, true);
    if (!res) return res;

    slot->status.status = "ok";
    slot->status.activated_timestamp = current_timestamp();
    slot->status.activated_count++;

    if (!ctx.config().data_directory.empty())
        save_slot_status(*slot, ctx.config().data_directory);
    if (!ctx.config().statusfile.empty())
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);

    LOG_INFO("Marked slot %s as good", slot->name.c_str());
    return Result<void>::ok();
}

Result<void> mark_bad(const std::string& slot_identifier) {
    auto& ctx = Context::instance();
    auto* slot = resolve_slot(slot_identifier);

    auto bootchooser = create_bootchooser(ctx.config());
    auto res = bootchooser->set_state(*slot, false);
    if (!res) return res;

    slot->status.status = "bad";

    if (!ctx.config().data_directory.empty())
        save_slot_status(*slot, ctx.config().data_directory);
    if (!ctx.config().statusfile.empty())
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);

    LOG_INFO("Marked slot %s as bad", slot->name.c_str());
    return Result<void>::ok();
}

Result<void> mark_active(const std::string& slot_identifier) {
    auto& ctx = Context::instance();
    auto* slot = resolve_slot(slot_identifier);

    auto bootchooser = create_bootchooser(ctx.config());
    auto res = bootchooser->set_primary(*slot);
    if (!res) return res;

    slot->status.activated_timestamp = current_timestamp();
    slot->status.activated_count++;

    if (!ctx.config().data_directory.empty())
        save_slot_status(*slot, ctx.config().data_directory);
    if (!ctx.config().statusfile.empty())
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);

    LOG_INFO("Marked slot %s as active (primary boot target)", slot->name.c_str());
    return Result<void>::ok();
}

} // namespace rauc
