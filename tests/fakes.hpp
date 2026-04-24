#pragma once

#include "aegis/bootloader/boot_control.hpp"
#include "aegis/core/gcs_client.hpp"

namespace aegis::test {

class FakeBootControl : public IBootControl {
public:
    mutable std::string booted{"A"};
    mutable std::string primary{"A"};
    bool bootable{true};

    std::string getBootedSlot() const override { return booted; }
    std::string getPrimarySlot() const override { return primary; }
    std::string getInactiveSlot() const override { return booted == "A" ? "B" : "A"; }
    bool isSlotBootable(const std::string&) const override { return bootable; }
    void setSlotBootable(const std::string&, bool) const override {}
    void setPrimarySlot(const std::string& slot) const override { primary = slot; }
    void markGood(const std::string&) const override {}
    void markBad(const std::string&) const override {}
};

class FakeGcsClient : public IGcsClient {
public:
    std::optional<GcsUpdateInfo> checkForUpdate() override { return std::nullopt; }
    void reportStatus(const OtaStatus&) override {}
};

}  // namespace aegis::test
