#pragma once

#include "aegis/bootloader/boot_control.hpp"
#include "aegis/core/types.hpp"

namespace aegis {
class UBootControl : public IBootControl {
public:
    UBootControl(CommandRunner runner);

    std::string getBootedSlot() const override;
    std::string getPrimarySlot() const override;
    std::string getInactiveSlot() const override;
    bool isSlotBootable(const std::string& slot) const override;
    void setSlotBootable(const std::string& slot, bool bootable) const override;
    void setPrimarySlot(const std::string& slot) const override;
    void markGood(const std::string& slot) const override;
    void markBad(const std::string& slot) const override;

private:
    std::string printEnv(const std::string& name) const;
    void setEnv(const std::string& name, const std::string& value) const;
    std::string statusVar(const std::string& slot) const;

    CommandRunner runner_;
};

} // namespace aegis 