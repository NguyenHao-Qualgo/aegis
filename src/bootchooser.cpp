#include "aegis/bootchooser.h"
#include "aegis/utils.h"

#include <cctype>

namespace aegis {

namespace {

void trim_trailing_newlines(std::string& value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
}

} // namespace

Result<std::string> UBootBootchooser::env_get(const std::string& key) {
    auto res = run_command({"fw_printenv", "-n", key});
    if (res.first != 0)
        return Result<std::string>::err("fw_printenv failed for " + key);
    auto val = res.second;
    trim_trailing_newlines(val);
    return Result<std::string>::ok(std::move(val));
}

Result<void> UBootBootchooser::env_set(const std::string& key, const std::string& value) {
    auto res = run_command({"fw_setenv", key, value});
    if (res.first != 0)
        return Result<void>::err("fw_setenv failed for " + key + "=" + value);
    return Result<void>::ok();
}

Slot* UBootBootchooser::get_primary(std::map<std::string, Slot>& slots) {
    auto chain = env_get("Bootchain");
    if (!chain) {
        LOG_WARNING("Cannot read Bootchain from U-Boot env");
        return nullptr;
    }

    // Bootchain "A" → bootname "a",  "B" → bootname "b"
    const std::string bootname(1, static_cast<char>(std::tolower(chain.value().front())));
    for (auto& [name, slot] : slots) {
        if (slot.bootname == bootname)
            return &slot;
    }
    return nullptr;
}

Result<void> UBootBootchooser::set_primary(Slot& slot) {
    if (slot.bootname != "a" && slot.bootname != "b")
        return Result<void>::err("UBoot bootchooser requires bootname 'a' or 'b', got: '" +
                                 slot.bootname + "'");

    const std::string chain(1, static_cast<char>(std::toupper(slot.bootname.front())));
    return env_set("Bootchain", chain);
}

Result<bool> UBootBootchooser::get_state(const Slot& slot) {
    auto val = env_get(status_var(slot));
    if (!val)
        return Result<bool>::err(val.error());
    return Result<bool>::ok(val.value() == "1");
}

Result<void> UBootBootchooser::set_state(Slot& slot, bool good) {
    return env_set(status_var(slot), good ? "1" : "0");
}

CustomBootchooser::CustomBootchooser(std::string backend_script)
    : backend_script_(std::move(backend_script)) {}

Result<std::string> CustomBootchooser::run_backend(const std::string& command,
                                                   const std::string& bootname) {
    auto res = run_command({backend_script_, command, bootname});
    if (res.first != 0)
        return Result<std::string>::err("Custom backend '" + command +
                                        "' failed: " + res.second);
    auto val = res.second;
    trim_trailing_newlines(val);
    return Result<std::string>::ok(std::move(val));
}

Result<void> CustomBootchooser::run_backend_set(const std::string& command,
                                                const std::string& bootname,
                                                const std::string& value) {
    auto res = run_command({backend_script_, command, bootname, value});
    if (res.first != 0)
        return Result<void>::err("Custom backend '" + command + "' set failed: " + res.second);
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

std::unique_ptr<IBootchooser> create_bootchooser(const SystemConfig& config) {
    switch (config.bootloader) {
    case Bootloader::UBoot:
        return std::make_unique<UBootBootchooser>();
    case Bootloader::Custom:
        return std::make_unique<CustomBootchooser>(config.handler_bootloader_custom_backend);
    case Bootloader::Noop:
        throw BootError("Noop bootloader has no bootchooser");
    }
    throw BootError("Unknown bootloader type");
}

} // namespace aegis
