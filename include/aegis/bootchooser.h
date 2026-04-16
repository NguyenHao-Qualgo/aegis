#pragma once

#include "aegis/config_file.h"
#include "aegis/error.h"
#include "aegis/slot.h"

#include <cctype>
#include <memory>
#include <string>

namespace aegis {

/// Abstract bootchooser backend interface.
class IBootchooser {
  public:
    virtual ~IBootchooser() = default;

    /// Get the primary (next-boot) slot
    virtual Slot* get_primary(std::map<std::string, Slot>& slots) = 0;

    /// Set a slot as the primary boot target
    virtual Result<void> set_primary(Slot& slot) = 0;

    /// Get the bootable state of a slot (true = bootable)
    virtual Result<bool> get_state(const Slot& slot) = 0;

    /// Mark a slot bootable or unbootable
    virtual Result<void> set_state(Slot& slot, bool good) = 0;
};

/// U-Boot bootchooser via fw_setenv / fw_printenv.
///
/// Variable scheme:
///   Bootchain    : "A" = slot-a active, "B" = slot-b active
///   RootAStatus  : 0 = unbootable, 1 = bootable  (slot with bootname "a")
///   RootBStatus  : 0 = unbootable, 1 = bootable  (slot with bootname "b")
///
/// Slot bootnames must be "a" or "b" in system.conf.
class UBootBootchooser : public IBootchooser {
  public:
    Slot* get_primary(std::map<std::string, Slot>& slots) override;
    Result<void> set_primary(Slot& slot) override;
    Result<bool> get_state(const Slot& slot) override;
    Result<void> set_state(Slot& slot, bool good) override;

  private:
    Result<std::string> env_get(const std::string& key);
    Result<void> env_set(const std::string& key, const std::string& value);

    /// Returns "RootAStatus" or "RootBStatus" based on slot.bootname
    static std::string status_var(const Slot& slot) {
        const char letter = static_cast<char>(std::toupper(slot.bootname.front()));
        return std::string("Root") + letter + "Status";
    }
};

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

/// Factory: create the appropriate bootchooser from config
std::unique_ptr<IBootchooser> create_bootchooser(const SystemConfig& config);

} // namespace aegis
