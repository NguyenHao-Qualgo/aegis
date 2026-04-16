#include "aegis/mark.h"

#include "aegis/context.h"
#include "aegis/slot.h"
#include "aegis/status_file.h"
#include "aegis/utils.h"

namespace aegis {

namespace {

Result<Slot*> resolve_slot(const std::string& identifier) {
    auto& ctx = Context::instance();
    auto& slots = ctx.config().slots;

    auto find_booted = [&]() -> Slot* {
        for (auto& [name, slot] : slots) {
            if (slot.is_booted) {
                return &slot;
            }
        }
        return nullptr;
    };

    if (identifier.empty() || identifier == "booted") {
        if (auto* slot = find_booted()) {
            return Result<Slot*>::ok(slot);
        }
        return Result<Slot*>::err("Cannot determine booted slot");
    }

    if (identifier == "other") {
        auto* booted = find_booted();
        if (!booted) {
            return Result<Slot*>::err("Cannot determine booted slot");
        }

        for (auto& [name, slot] : slots) {
            if (slot.readonly) {
                continue;
            }
            if (slot.slot_class == booted->slot_class && slot.index != booted->index) {
                return Result<Slot*>::ok(&slot);
            }
        }

        return Result<Slot*>::err("Cannot determine other slot");
    }

    auto it = slots.find(identifier);
    if (it != slots.end()) {
        return Result<Slot*>::ok(&it->second);
    }

    for (auto& [name, slot] : slots) {
        if (slot.bootname == identifier) {
            return Result<Slot*>::ok(&slot);
        }
    }

    return Result<Slot*>::err("Slot not found: " + identifier);
}

Result<void> save_slot_metadata() {
    auto& ctx = Context::instance();

    if (!ctx.config().data_directory.empty()) {
        for (auto& [name, slot] : ctx.config().slots) {
            auto res = save_slot_status(slot, ctx.config().data_directory);
            if (!res) {
                return res;
            }
        }
    }

    if (!ctx.config().statusfile.empty()) {
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);
    }

    return Result<void>::ok();
}

} // namespace

Result<void> mark_good(const std::string& slot_identifier) {
    auto& ctx = Context::instance();

    auto slot_res = resolve_slot(slot_identifier);
    if (!slot_res) {
        return Result<void>::err(slot_res.error());
    }
    auto* slot = slot_res.value();

    auto res = ctx.bootchooser().set_state(*slot, true);
    if (!res) {
        return res;
    }

    slot->status.status = "ok";

    if (!ctx.config().data_directory.empty()) {
        auto save_res = save_slot_status(*slot, ctx.config().data_directory);
        if (!save_res) {
            return save_res;
        }
    }
    if (!ctx.config().statusfile.empty()) {
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);
    }

    LOG_INFO("Marked slot %s as good", slot->name.c_str());
    return Result<void>::ok();
}

Result<void> mark_bad(const std::string& slot_identifier) {
    auto& ctx = Context::instance();

    auto slot_res = resolve_slot(slot_identifier);
    if (!slot_res) {
        return Result<void>::err(slot_res.error());
    }
    auto* slot = slot_res.value();

    auto res = ctx.bootchooser().set_state(*slot, false);
    if (!res) {
        return res;
    }

    slot->status.status = "bad";

    if (!ctx.config().data_directory.empty()) {
        auto save_res = save_slot_status(*slot, ctx.config().data_directory);
        if (!save_res) {
            return save_res;
        }
    }
    if (!ctx.config().statusfile.empty()) {
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);
    }

    LOG_INFO("Marked slot %s as bad", slot->name.c_str());
    return Result<void>::ok();
}

Result<void> mark_active(const std::string& slot_identifier) {
    auto& ctx = Context::instance();

    auto slot_res = resolve_slot(slot_identifier);
    if (!slot_res) {
        return Result<void>::err(slot_res.error());
    }
    auto* slot = slot_res.value();

    auto res = ctx.bootchooser().set_primary(*slot);
    if (!res) {
        return res;
    }

    slot->status.activated_timestamp = current_timestamp();
    slot->status.activated_count++;

    if (!ctx.config().data_directory.empty()) {
        auto save_res = save_slot_status(*slot, ctx.config().data_directory);
        if (!save_res) {
            return save_res;
        }
    }
    if (!ctx.config().statusfile.empty()) {
        save_all_slot_status(ctx.config().slots, ctx.config().statusfile);
    }

    LOG_INFO("Marked slot %s as active (primary boot target)", slot->name.c_str());
    return Result<void>::ok();
}

} // namespace aegis