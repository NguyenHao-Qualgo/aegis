#include "aegis/bootchooser.h"
#include "aegis/utils.h"

#include <sstream>

namespace aegis {

Result<std::string> UBootBootchooser::env_get(const std::string& key) {
    auto res = run_command({"fw_printenv", "-n", key});
    if (res.exit_code != 0)
        return Result<std::string>::err("fw_printenv failed for " + key);
    auto val = res.stdout_str;
    while (!val.empty() && (val.back() == '\n' || val.back() == '\r'))
        val.pop_back();
    return Result<std::string>::ok(std::move(val));
}

Result<void> UBootBootchooser::env_set(const std::string& key, const std::string& value) {
    auto res = run_command({"fw_setenv", key, value});
    if (res.exit_code != 0)
        return Result<void>::err("fw_setenv failed for " + key + "=" + value);
    return Result<void>::ok();
}

Slot* UBootBootchooser::get_primary(std::map<std::string, Slot>& slots) {
    auto order_result = env_get("BOOT_ORDER");
    if (!order_result) {
        LOG_WARNING("Cannot read BOOT_ORDER from U-Boot env");
        return nullptr;
    }

    // BOOT_ORDER is space-separated list of bootnames, first = primary
    std::istringstream iss(order_result.value());
    std::string first_bootname;
    iss >> first_bootname;

    for (auto& [name, slot] : slots) {
        if (slot.bootname == first_bootname)
            return &slot;
    }
    return nullptr;
}

Result<void> UBootBootchooser::set_primary(Slot& slot) {
    if (slot.bootname.empty())
        return Result<void>::err("Slot has no bootname: " + slot.name);

    // Read current order, move this slot to front
    auto order_result = env_get("BOOT_ORDER");
    std::string new_order = slot.bootname;
    if (order_result) {
        std::istringstream iss(order_result.value());
        std::string name;
        while (iss >> name) {
            if (name != slot.bootname)
                new_order += " " + name;
        }
    }

    auto res = env_set("BOOT_ORDER", new_order);
    if (!res)
        return res;

    // Reset boot count
    res = env_set(slot.bootname + "_OK", "0");
    if (!res)
        return res;
    res = env_set(slot.bootname + "_TRY", "0");
    return res;
}

Result<bool> UBootBootchooser::get_state(const Slot& slot) {
    auto ok_result = env_get(slot.bootname + "_OK");
    if (!ok_result)
        return Result<bool>::err(ok_result.error());
    return Result<bool>::ok(ok_result.value() == "1");
}

Result<void> UBootBootchooser::set_state(Slot& slot, bool good) {
    auto res = env_set(slot.bootname + "_OK", good ? "1" : "0");
    if (!res)
        return res;
    if (good) {
        res = env_set(slot.bootname + "_TRY", "0");
    }
    return res;
}

CustomBootchooser::CustomBootchooser(std::string backend_script)
    : backend_script_(std::move(backend_script)) {}

Result<std::string> CustomBootchooser::run_backend(const std::string& command,
                                                   const std::string& bootname) {
    auto res = run_command({backend_script_, command, bootname});
    if (res.exit_code != 0)
        return Result<std::string>::err("Custom backend '" + command +
                                        "' failed: " + res.stderr_str);
    auto val = res.stdout_str;
    while (!val.empty() && (val.back() == '\n' || val.back() == '\r'))
        val.pop_back();
    return Result<std::string>::ok(std::move(val));
}

Result<void> CustomBootchooser::run_backend_set(const std::string& command,
                                                const std::string& bootname,
                                                const std::string& value) {
    auto res = run_command({backend_script_, command, bootname, value});
    if (res.exit_code != 0)
        return Result<void>::err("Custom backend '" + command + "' set failed: " + res.stderr_str);
    return Result<void>::ok();
}

Slot* CustomBootchooser::get_primary(std::map<std::string, Slot>& slots) {
    auto res = run_backend("get-primary", "");
    if (!res)
        return nullptr;
    for (auto& [name, slot] : slots) {
        if (slot.bootname == res.value())
            return &slot;
    }
    return nullptr;
}

Result<void> CustomBootchooser::set_primary(Slot& slot) {
    return run_backend_set("set-primary", slot.bootname, "");
}

Result<bool> CustomBootchooser::get_state(const Slot& slot) {
    auto res = run_backend("get-state", slot.bootname);
    if (!res)
        return Result<bool>::err(res.error());
    return Result<bool>::ok(res.value() == "good");
}

Result<void> CustomBootchooser::set_state(Slot& slot, bool good) {
    return run_backend_set("set-state", slot.bootname, good ? "good" : "bad");
}

Slot* NoopBootchooser::get_primary(std::map<std::string, Slot>& slots) {
    for (auto& [name, slot] : slots) {
        if (slot.is_booted)
            return &slot;
    }
    return nullptr;
}

Result<void> NoopBootchooser::set_primary(Slot& slot) {
    LOG_INFO("noop bootchooser: ignoring set_primary for %s", slot.name.c_str());
    return Result<void>::ok();
}

Result<bool> NoopBootchooser::get_state(const Slot& slot) {
    return Result<bool>::ok(true);
}

Result<void> NoopBootchooser::set_state(Slot& slot, bool good) {
    LOG_INFO("noop bootchooser: ignoring set_state %s for %s", good ? "good" : "bad",
             slot.name.c_str());
    return Result<void>::ok();
}

std::unique_ptr<IBootchooser> create_bootchooser(const SystemConfig& config) {
    switch (config.bootloader) {
    case Bootloader::UBoot:
        return std::make_unique<UBootBootchooser>();
    case Bootloader::Custom:
        return std::make_unique<CustomBootchooser>(config.handler_bootloader_custom_backend);
    case Bootloader::Noop:
        return std::make_unique<NoopBootchooser>();
    }
    throw BootError("Unknown bootloader type");
}

} // namespace aegis
