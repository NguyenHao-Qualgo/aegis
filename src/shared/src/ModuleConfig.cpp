#include "ModuleConfig.h"

#include "utils.h"

ModuleConfig::ModuleConfig() {
    LoadConfigFile(Env::IoTHubConfigName);
}

bool ModuleConfig::LoadConfigFile(const std::string& fileName) {
    std::lock_guard<std::mutex> lock(mutex_);

    _configFilePath = Env::ConfigBaseDirectory / fileName;
    if (!std::filesystem::exists(_configFilePath)) {
        if (!GenerateDefaultConfig()) {
            LOG_E("Failed to generate default config file");
            return false;
        }
    }

    try {
        std::string content = ReadFileContent(_configFilePath);
        if (content.empty()) {
            LOG_W("Config file is empty, generating default config");
            return GenerateDefaultConfig();
        }

        _config = nlohmann::json::parse(content);
        return !_config.empty();
    } catch (const std::exception& e) {
        LOG_E("Failed to load config file '{}': {}", _configFilePath, e.what());
        LOG_I("Attempting to generate default config");
        return GenerateDefaultConfig();
    }
}

bool ModuleConfig::GenerateDefaultConfig() {
    nlohmann::json defaultConfig = {{"IotHubConnectionString", ""}, {"KeepAliveSecs", Env::KeepAliveSecs},
        {"RecoverIotHubSecs", Env::RecoverIotHubSecs}, {"cert_path", ""}, {"priv_key", ""}, {"log_level", 4},
        {"host_name", ""}, {"device_id", ""}, {"last_module_state", false}};

    try {
        if (!std::filesystem::exists(Env::ConfigBaseDirectory)) {
            std::filesystem::create_directories(Env::ConfigBaseDirectory);
        }

        WriteFileContent(_configFilePath, defaultConfig.dump(4));
        _config = defaultConfig;
        return true;
    } catch (const std::exception& e) {
        LOG_E("Failed to generate default config: {}", e.what());
        return false;
    }
}

bool ModuleConfig::SaveConfigFile() {
    std::lock_guard<std::mutex> lock(mutex_);
    return SaveConfigFileInternal();
}

bool ModuleConfig::SaveConfigFileInternal() {
    // Note: This method assumes mutex is already locked
    try {
        WriteFileContent(_configFilePath, _config.dump(4));
        return true;
    } catch (const std::exception& e) {
        LOG_E("Error saving config file '{}': {}", _configFilePath, e.what());
        return false;
    }
}

bool ModuleConfig::RemoveProperty(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        if (_config.contains(key)) {
            _config.erase(key);
            return SaveConfigFileInternal();
        }
        LOG_W("Key '{}' not found in config", key);
        return false;
    } catch (const std::exception& e) {
        LOG_E("Error removing property '{}' from config: {}", key, e.what());
        return false;
    }
}