#pragma once

#include <string>

#include "aegis/command_runner.hpp"
#include "aegis/types.hpp"

namespace aegis {

class BootControl {
public:
    BootControl(OtaConfig config, CommandRunner runner);

    std::string getBootedSlot() const;
    std::string getPrimarySlot() const;
    std::string getInactiveSlot() const;
    bool isSlotBootable(const std::string& slot) const;
    void setSlotBootable(const std::string& slot, bool bootable) const;
    void setPrimarySlot(const std::string& slot) const;
    void markGood(const std::string& slot) const;
    void markBad(const std::string& slot) const;

private:
    std::string printEnv(const std::string& name) const;
    void setEnv(const std::string& name, const std::string& value) const;
    std::string statusVar(const std::string& slot) const;

    OtaConfig config_;
    CommandRunner runner_;
};

}  // namespace aegis
