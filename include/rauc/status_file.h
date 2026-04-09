#pragma once

#include "rauc/error.h"
#include "rauc/slot.h"

#include <map>
#include <string>

namespace rauc {

Result<void> load_slot_status(Slot& slot, const std::string& data_directory);

Result<void> save_slot_status(const Slot& slot, const std::string& data_directory);

Result<void> load_all_slot_status(std::map<std::string, Slot>& slots,
                                  const std::string& status_file_path);

Result<void> save_all_slot_status(const std::map<std::string, Slot>& slots,
                                  const std::string& status_file_path);

std::string current_timestamp();

} // namespace rauc
