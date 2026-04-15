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

    bool install_running() const;
    std::string operation() const;
    std::string last_error() const;
    ProgressInfo progress() const;

  private:
    mutable std::mutex mutex_;
    bool install_running_ = false;
    std::string operation_ = "idle";
    std::string last_error_;
    ProgressInfo progress_;
};

} // namespace aegis