#pragma once

#include <filesystem>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

#include "env.h"
#include "logging.h"

struct OnvifConf {};
struct GstreamerConf {};
class ModuleConfig {
   public:
    static ModuleConfig& GetInstance() {
        static ModuleConfig instance;
        return instance;
    }

    ModuleConfig(const ModuleConfig&) = delete;
    ModuleConfig& operator=(const ModuleConfig&) = delete;

    bool GenerateDefaultConfig();
    bool SaveConfigFile();

    template <typename T>
    bool GetProperty(const std::string& key, T& value, const T& default_value) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (_config.contains(key)) {
                value = _config[key].get<T>();
                return true;
            }
            if (SetPropertyInternal(key, default_value)) {
                LOG_I("Re-insert default {}", key);
                value = default_value;
                return true;
            }
            LOG_W("Key '{}' not found in config", key);
            return false;
        } catch (const std::exception& e) {
            LOG_E("Error getting property '{}' from config: {}", key, e.what());
            return false;
        }
    }

    template <typename T>
    bool GetProperty(const std::string& key, T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (_config.contains(key)) {
                value = _config[key].get<T>();
                return true;
            }
            LOG_W("Key '{}' not found in config", key);
            return false;
        } catch (const std::exception& e) {
            LOG_E("Error getting property '{}' from config: {}", key, e.what());
            return false;
        }
    }

    template <typename T>
    bool SetProperty(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        return SetPropertyInternal(key, value);
    }

    bool RemoveProperty(const std::string& key);

   private:
    ModuleConfig();
    bool LoadConfigFile(const std::string& fileName);

    // Internal methods that assume mutex is already locked
    template <typename T>
    bool SetPropertyInternal(const std::string& key, const T& value) {
        try {
            if (_config.contains(key) && _config[key] == value) {
                return true;
            }
            _config[key] = value;
            return SaveConfigFileInternal();
        } catch (const std::exception& e) {
            LOG_E("Error setting property '{}' in config: {}", key, e.what());
            return false;
        }
    }

    bool SaveConfigFileInternal();

    mutable std::mutex mutex_;
    std::string _configFilePath;
    nlohmann::json _config;
};