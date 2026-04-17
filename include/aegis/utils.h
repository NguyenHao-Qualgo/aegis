#pragma once

#include "aegis/error.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aegis {

std::pair<int, std::string> run_command(const std::vector<std::string>& argv,
                                        const std::vector<std::string>& env = {});

bool path_exists(const std::string& path);

uint64_t file_size(const std::string& path);

Result<void> mkdir_p(const std::string& path);

Result<void> rm_rf(const std::string& path);

Result<void> copy_file(const std::string& src, const std::string& dst);

std::string read_text_file(const std::string& path);

void write_text_file(const std::string& path, const std::string& content);

std::string random_hex(size_t bytes);

std::string resolve_path(const std::string& base_dir, const std::string& rel_path);

std::string dirname(const std::string& path);

std::string basename(const std::string& path);

enum class LogLevel { Debug, Info, Warning, Error };

void log(LogLevel level, const char* fmt, ...);

#define LOG_DEBUG(...) ::aegis::log(::aegis::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::aegis::log(::aegis::LogLevel::Info, __VA_ARGS__)
#define LOG_WARNING(...) ::aegis::log(::aegis::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...) ::aegis::log(::aegis::LogLevel::Error, __VA_ARGS__)

} // namespace aegis