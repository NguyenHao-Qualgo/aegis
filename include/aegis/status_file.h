#pragma once

#include "aegis/config_file.h"
#include "aegis/error.h"
#include "aegis/slot.h"

#include <map>
#include <string>

namespace aegis {

Result<void> load_slot_status(Slot& slot, const std::string& data_directory);

Result<void> save_slot_status(const Slot& slot, const std::string& data_directory);

Result<void> load_all_slot_status(std::map<std::string, Slot>& slots,
                                  const std::string& status_file_path);

Result<void> save_all_slot_status(const std::map<std::string, Slot>& slots,
                                  const std::string& status_file_path);

std::string current_timestamp();

class IStatusStore {
  public:
    virtual ~IStatusStore() = default;
    virtual Result<void> save_slot(const Slot& slot) = 0;
    virtual Result<void> save_all(const std::map<std::string, Slot>& slots) = 0;
};

class FileStatusStore : public IStatusStore {
  public:
    explicit FileStatusStore(const SystemConfig& config) : config_(config) {}

    Result<void> save_slot(const Slot& slot) override;
    Result<void> save_all(const std::map<std::string, Slot>& slots) override;

  private:
    const SystemConfig& config_;
};

} // namespace aegis
