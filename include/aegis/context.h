#pragma once

#include "aegis/config_file.h"

#include <memory>
#include <string>

namespace aegis {

class Context {
  public:
    static Context& instance();

    /// Initialize from command-line options
    void init(const std::string& config_path, const std::string& cert_path = {},
              const std::string& key_path = {}, const std::string& keyring_path = {},
              const std::string& override_boot_slot = {}, const std::string& mount_prefix = {});

    /// Accessors
    [[nodiscard]] const SystemConfig& config() const {
        return config_;
    }
    SystemConfig& config() {
        return config_;
    }

    [[nodiscard]] const std::string& config_path() const {
        return config_path_;
    }
    [[nodiscard]] const std::string& cert_path() const {
        return cert_path_;
    }
    [[nodiscard]] const std::string& key_path() const {
        return key_path_;
    }
    [[nodiscard]] const std::string& keyring_path() const {
        return keyring_path_;
    }
    [[nodiscard]] const std::string& boot_slot() const {
        return boot_slot_;
    }
    [[nodiscard]] const std::string& mount_prefix() const {
        return mount_prefix_;
    }

    [[nodiscard]] bool is_initialized() const {
        return initialized_;
    }

  public:
    Context() = default;
    ~Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

  private:

    bool initialized_ = false;
    std::string config_path_;
    std::string cert_path_;
    std::string key_path_;
    std::string keyring_path_;
    std::string boot_slot_;
    std::string mount_prefix_;
    SystemConfig config_;
};

} // namespace aegis
