#pragma once

#include "aegis/progress_info.h"

#include <mutex>
#include <string>

namespace aegis {

class ServiceState {
  public:
    void start_installing();
    void finish_install(int result, std::string last_error);
    void update_progress(int percentage, std::string message, int depth);
    void update_ota(std::string ota_state, std::string ota_status_message,
                    std::string transaction_id, std::string expected_slot);
    void set_last_error(std::string last_error);

    bool install_running() const;
    std::string operation() const;
    std::string last_error() const;
    ProgressInfo progress() const;
    std::string ota_state() const;
    std::string ota_status_message() const;
    std::string transaction_id() const;
    std::string expected_slot() const;

  private:
    mutable std::mutex mutex_;
    bool install_running_ = false;
    std::string operation_ = "idle";
    std::string last_error_;
    ProgressInfo progress_;
    std::string ota_state_ = "idle-sync";
    std::string ota_status_message_ = "idle";
    std::string transaction_id_;
    std::string expected_slot_;
};

} // namespace aegis