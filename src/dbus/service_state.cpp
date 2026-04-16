#include "aegis/dbus/service_state.h"

namespace aegis {

void ServiceState::start_installing() {
    std::lock_guard<std::mutex> lock(mutex_);
    install_running_ = true;
    operation_ = "installing";
    last_error_.clear();
    progress_ = {0, "Starting installation", 0};
}

void ServiceState::finish_install(int result, std::string last_error) {
    std::lock_guard<std::mutex> lock(mutex_);
    install_running_ = false;
    operation_ = "idle";
    last_error_ = std::move(last_error);

    if (result != 0 && progress_.message.empty()) {
        progress_.message = "Installation failed";
    }
}

void ServiceState::update_progress(int percentage, std::string message, int depth) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_.percentage = percentage;
    progress_.message = std::move(message);
    progress_.depth = depth;
}

void ServiceState::update_ota(std::string ota_state, std::string ota_status_message,
                              std::string transaction_id, std::string expected_slot) {
    std::lock_guard<std::mutex> lock(mutex_);
    ota_state_ = std::move(ota_state);
    ota_status_message_ = std::move(ota_status_message);
    transaction_id_ = std::move(transaction_id);
    expected_slot_ = std::move(expected_slot);
}

void ServiceState::set_last_error(std::string last_error) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_ = std::move(last_error);
}

bool ServiceState::install_running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return install_running_;
}

std::string ServiceState::operation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return operation_;
}

std::string ServiceState::last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

ProgressInfo ServiceState::progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string ServiceState::ota_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ota_state_;
}

std::string ServiceState::ota_status_message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ota_status_message_;
}

std::string ServiceState::transaction_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transaction_id_;
}

std::string ServiceState::expected_slot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return expected_slot_;
}

} // namespace aegis
