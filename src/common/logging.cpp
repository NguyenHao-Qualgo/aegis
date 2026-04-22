#include "aegis/common/logging.hpp"

#include <spdlog/sinks/dup_filter_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

std::shared_ptr<spdlog::logger> AppLog::s_AppLogger = nullptr;
spdlog::level::level_enum AppLog::s_Level = spdlog::level::info;

void AppLog::Init(
    AppLog::Level level, const char* file_path, const std::string& app_name, size_t file_count, size_t file_size) {
    switch (level) {
        case AppLog::Level::off:
            s_Level = spdlog::level::off;
            break;
        case AppLog::Level::critical:
            s_Level = spdlog::level::critical;
            break;
        case AppLog::Level::err:
            s_Level = spdlog::level::err;
            break;
        case AppLog::Level::warn:
            s_Level = spdlog::level::warn;
            break;
        case AppLog::Level::info:
            s_Level = spdlog::level::info;
            break;
        case AppLog::Level::debug:
            s_Level = spdlog::level::debug;
            break;
        case AppLog::Level::trace:
            [[fallthrough]];  // no warning on fallthrough
        default:
            s_Level = spdlog::level::trace;
            break;
    }

    const std::string starting_line =
        fmt::format("\n\tInitialized spdlog version: {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    // duplicate filter
    const auto dup_filter = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(
        std::chrono::seconds(APPLOG_CONFIG_DUPLICATE_FILTER_TIMER_S));
    const std::string duplicate_info =
        fmt::format("\n\tDuplicate filter timer: {} seconds", APPLOG_CONFIG_DUPLICATE_FILTER_TIMER_S);
    const size_t bt_size = APPLOG_CONFIG_BACKTRACE_SIZE;
    // Logger
    const spdlog::sink_ptr console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    const std::string console_pattern = "[" + app_name + "] [%L] [thread %t] %v";
    console_sink->set_pattern(console_pattern);
    dup_filter->add_sink(console_sink);
    std::string file_info;
    if (file_path != nullptr && file_count > 0) {
        const spdlog::sink_ptr file_sink =
            std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file_path, file_size, file_count, false);
        file_sink->set_pattern(APPLOG_CONFIG_FORMAT_FILE);
        dup_filter->add_sink(file_sink);
        file_info = fmt::format("\n\tFile log: {}, max size: {}, total files: {}.", file_path, file_size, file_count);
    }
    s_AppLogger = std::make_shared<spdlog::logger>("AppLogger", dup_filter);
    s_AppLogger->set_level(spdlog::level::trace);
    for (const spdlog::sink_ptr& sink : s_AppLogger->sinks()) {
        sink->set_level(s_Level);
    }
    const std::string backtrace_info = fmt::format("\n\tBacktrace size: {}", bt_size);
    s_AppLogger->enable_backtrace(bt_size);
    s_AppLogger->info("\n^^^^^^^^^^^^^^^^^^^^^{}{}{}{}\n", starting_line, duplicate_info, backtrace_info, file_info);
    s_AppLogger->flush_on(s_Level);
    s_AppLogger->flush();
}

void AppLog::Flush() {
    s_AppLogger->flush();
}

void AppLog::DumpBacktrace() {
    if (s_AppLogger == nullptr)
        return;
    for (const spdlog::sink_ptr& sink : s_AppLogger->sinks()) {
        sink->set_level(spdlog::level::trace);
    }
    s_AppLogger->dump_backtrace();
    s_AppLogger->flush();
    for (const spdlog::sink_ptr& sink : s_AppLogger->sinks()) {
        sink->set_level(s_Level);
    }
}