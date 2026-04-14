#pragma once

#include "aegis/error.h"

#include <string>
#include <vector>
#include <cstdint>

namespace aegis {

/// Run a subprocess and capture stdout
struct SubprocessResult {
    int         exit_code;
    std::string stdout_str;
    std::string stderr_str;
};

SubprocessResult run_command(const std::vector<std::string>& argv,
                             const std::vector<std::string>& env = {});

/// Run a subprocess, check exit code, throw on failure
SubprocessResult run_command_checked(const std::vector<std::string>& argv,
                                     const std::vector<std::string>& env = {});

/// Check if a file/directory exists
bool path_exists(const std::string& path);

/// Get file size
uint64_t file_size(const std::string& path);

/// Create directory (and parents)
Result<void> mkdir_p(const std::string& path);

/// Remove directory recursively
Result<void> rm_rf(const std::string& path);

/// Copy a file
Result<void> copy_file(const std::string& src, const std::string& dst);

/// Read a text file into a string
std::string read_text_file(const std::string& path);

/// Write a string to a text file
void write_text_file(const std::string& path, const std::string& content);

/// Generate a random hex string
std::string random_hex(size_t bytes);

/// Resolve a relative path against a base directory
std::string resolve_path(const std::string& base_dir, const std::string& rel_path);

/// Get the directory part of a path
std::string dirname(const std::string& path);

/// Get the filename part of a path
std::string basename(const std::string& path);

/// Logging helpers
enum class LogLevel { Debug, Info, Warning, Error };

void log(LogLevel level, const char* fmt, ...);

#define LOG_DEBUG(...)   ::aegis::log(::aegis::LogLevel::Debug,   __VA_ARGS__)
#define LOG_INFO(...)    ::aegis::log(::aegis::LogLevel::Info,    __VA_ARGS__)
#define LOG_WARNING(...) ::aegis::log(::aegis::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...)   ::aegis::log(::aegis::LogLevel::Error,   __VA_ARGS__)

} // namespace aegis
