#pragma once

#include <string>
#include <memory>

#include "aegis/io/command_runner.hpp"
#include "aegis/core/types.hpp"

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

}  // namespace aegis
