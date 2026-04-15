#pragma once

#include "aegis/config_file.h"
#include "aegis/error.h"
#include "aegis/slot.h"

#include <memory>
#include <string>

namespace aegis {

/// Abstract bootchooser backend interface.
/// Only U-Boot and Custom are implemented (grub, barebox, efi removed).
class IBootchooser {
  public:
    virtual ~IBootchooser() = default;

    /// Get the primary (next-boot) slot
    virtual Slot* get_primary(std::map<std::string, Slot>& slots) = 0;

    /// Set a slot as the primary boot target
    virtual Result<void> set_primary(Slot& slot) = 0;

    /// Get the "good" state of a slot (ok = stable, not pending)
    virtual Result<bool> get_state(const Slot& slot) = 0;

    /// Mark a slot as good or bad
    virtual Result<void> set_state(Slot& slot, bool good) = 0;
};

/// U-Boot bootchooser via fw_setenv / fw_printenv
class UBootBootchooser : public IBootchooser {
  public:
    Slot* get_primary(std::map<std::string, Slot>& slots) override;
    Result<void> set_primary(Slot& slot) override;
    Result<bool> get_state(const Slot& slot) override;
    Result<void> set_state(Slot& slot, bool good) override;

  private:
    Result<std::string> env_get(const std::string& key);
    Result<void> env_set(const std::string& key, const std::string& value);
};

/// Custom bootchooser calling user-provided backend script
class CustomBootchooser : public IBootchooser {
  public:
    explicit CustomBootchooser(std::string backend_script);

    Slot* get_primary(std::map<std::string, Slot>& slots) override;
    Result<void> set_primary(Slot& slot) override;
    Result<bool> get_state(const Slot& slot) override;
    Result<void> set_state(Slot& slot, bool good) override;

  private:
    std::string backend_script_;

    Result<std::string> run_backend(const std::string& command, const std::string& bootname);
    Result<void> run_backend_set(const std::string& command, const std::string& bootname,
                                 const std::string& value);
};

/// No-op bootchooser for testing
class NoopBootchooser : public IBootchooser {
  public:
    Slot* get_primary(std::map<std::string, Slot>& slots) override;
    Result<void> set_primary(Slot& slot) override;
    Result<bool> get_state(const Slot& slot) override;
    Result<void> set_state(Slot& slot, bool good) override;
};

/// Factory: create the appropriate bootchooser from config
std::unique_ptr<IBootchooser> create_bootchooser(const SystemConfig& config);

} // namespace aegis
