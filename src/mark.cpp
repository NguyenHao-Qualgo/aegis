#include "aegis/mark.h"

#include "aegis/context.h"
#include "aegis/slot.h"
#include "aegis/status_file.h"
#include "aegis/utils.h"

namespace aegis {

namespace {

enum class SlotMark {
    Good,
    Bad,
    Active,
};

Result<Slot*> resolve_slot(std::map<std::string, Slot>& slots, const std::string& identifier) {
    if (identifier.empty() || identifier == "booted") {
        if (auto* slot = find_booted_slot(slots)) {
            return Result<Slot*>::ok(slot);
        }
        return Result<Slot*>::err("Cannot determine booted slot");
    }

    if (identifier == "other") {
        auto* booted = find_booted_slot(slots);
        if (!booted) {
            return Result<Slot*>::err("Cannot determine booted slot");
        }
        if (auto* slot = find_other_slot(slots, *booted)) {
            return Result<Slot*>::ok(slot);
        }
        return Result<Slot*>::err("Cannot determine other slot");
    }

    if (auto* slot = find_slot_by_identifier(slots, identifier)) {
        return Result<Slot*>::ok(slot);
    }

    return Result<Slot*>::err("Slot not found: " + identifier);
}

Result<void> apply_mark(Slot& slot, SlotMark mark, IBootchooser& bootchooser) {
    switch (mark) {
    case SlotMark::Good: {
        auto result = bootchooser.set_state(slot, true);
        if (!result) {
            return result;
        }
        slot.status.status = "ok";
        return Result<void>::ok();
    }
    case SlotMark::Bad: {
        auto result = bootchooser.set_state(slot, false);
        if (!result) {
            return result;
        }
        slot.status.status = "bad";
        return Result<void>::ok();
    }
    case SlotMark::Active: {
        auto result = bootchooser.set_primary(slot);
        if (!result) {
            return result;
        }
        slot.status.activated_timestamp = current_timestamp();
        slot.status.activated_count++;
        return Result<void>::ok();
    }
    }

    return Result<void>::err("Unknown mark operation");
}

const char* mark_name(SlotMark mark) {
    switch (mark) {
    case SlotMark::Good:
        return "good";
    case SlotMark::Bad:
        return "bad";
    case SlotMark::Active:
        return "active (primary boot target)";
    }
    return "unknown";
}

Result<void> mark_slot(const std::string& slot_identifier, SlotMark mark) {
    auto& ctx = Context::instance();
    auto slot_res = resolve_slot(ctx.config().slots, slot_identifier);
    if (!slot_res) {
        return Result<void>::err(slot_res.error());
    }

    auto* slot = slot_res.value();
    auto mark_result = apply_mark(*slot, mark, ctx.bootchooser());
    if (!mark_result) {
        return mark_result;
    }

    FileStatusStore status_store(ctx.config());
    auto save_result = status_store.save_slot(*slot);
    if (!save_result) {
        return save_result;
    }

    LOG_INFO("Marked slot %s as %s", slot->name.c_str(), mark_name(mark));
    return Result<void>::ok();
}

} // namespace

Result<void> mark_good(const std::string& slot_identifier) {
    return mark_slot(slot_identifier, SlotMark::Good);
}

Result<void> mark_bad(const std::string& slot_identifier) {
    return mark_slot(slot_identifier, SlotMark::Bad);
}

Result<void> mark_active(const std::string& slot_identifier) {
    return mark_slot(slot_identifier, SlotMark::Active);
}

} // namespace aegis
