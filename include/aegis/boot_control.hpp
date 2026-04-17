#pragma once

#include <string>

#include "aegis/command_runner.hpp"
#include "aegis/types.hpp"

namespace aegis {

class IBootControl {
public:
    virtual ~IBootControl() = default;

    virtual std::string getBootedSlot() const = 0;
    virtual std::string getPrimarySlot() const = 0;
    virtual std::string getInactiveSlot() const = 0;
    virtual bool isSlotBootable(const std::string& slot) const = 0;
    virtual void setSlotBootable(const std::string& slot, bool bootable) const = 0;
    virtual void setPrimarySlot(const std::string& slot) const = 0;
    virtual void markGood(const std::string& slot) const = 0;
    virtual void markBad(const std::string& slot) const = 0;
};

class BootControl : public IBootControl {
public:
    BootControl(OtaConfig config, CommandRunner runner);

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

    OtaConfig config_;
    CommandRunner runner_;
};

}  // namespace aegis
