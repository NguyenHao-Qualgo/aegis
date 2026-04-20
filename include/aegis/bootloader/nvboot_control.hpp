#pragma once

#include <string>

#include "aegis/bootloader/boot_control.hpp"

namespace aegis {

class NVBootControl : public IBootControl {
public:
    NVBootControl() = default;

    std::string getBootedSlot() const override;
    std::string getPrimarySlot() const override;
    std::string getInactiveSlot() const override;

    bool isSlotBootable(const std::string& slot) const override;
    void setSlotBootable(const std::string& slot, bool bootable) const override;

    void setPrimarySlot(const std::string& slot) const override;
    void markGood(const std::string& slot) const override;
    void markBad(const std::string& slot) const override;

private:
    static constexpr const char* kGuid = "781e084c-a330-417c-b678-38e696380cb9";

    static bool isValidSlotName(const std::string& slot);
    static void validateSlotName(const std::string& slot);
    static std::string otherSlot(const std::string& slot);
};

}  // namespace aegis
