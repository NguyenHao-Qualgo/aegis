#pragma once
#include <spdlog/spdlog.h>

#include <memory>

constexpr std::size_t APPLOG_CONFIG_DEFAULT_LOG_FILE_SIZE = 10 * 1024 * 1024;
constexpr std::size_t APPLOG_CONFIG_DEFAULT_LOG_FILE_COUNT = 2;
constexpr std::size_t APPLOG_CONFIG_BACKTRACE_SIZE = 200;
constexpr std::size_t APPLOG_CONFIG_DUPLICATE_FILTER_TIMER_S = 30;
constexpr const char* APPLOG_CONFIG_FORMAT_FILE = "[%Y-%m-%d %T] [%L] [thread %t] %v";

class AppLog {
   public:
    enum Level : int { off = 0, critical = 1, err = 2, warn = 3, info = 4, debug = 5, trace = 6 };
    AppLog() = default;
    AppLog(const AppLog&) = delete;
    AppLog& operator=(const AppLog&) = delete;
    AppLog& operator=(AppLog&&) = delete;
    ~AppLog() = default;
    static void Init(AppLog::Level level = AppLog::Level::info, const char* file_path = nullptr,
        const std::string& app_name = "UNKNOWN", size_t file_count = APPLOG_CONFIG_DEFAULT_LOG_FILE_COUNT,
        size_t file_size = APPLOG_CONFIG_DEFAULT_LOG_FILE_SIZE);
    inline static std::shared_ptr<spdlog::logger>& GetLogger() {
        if (s_AppLogger == nullptr)
            ::AppLog::Init();

        return s_AppLogger;
    };
    template <typename... Params>
    inline static std::string Format(const char* file, int line, const char* func, Params&&... params) {
        return fmt::format("{}:{}::{}(): {}", file, line, func, fmt::format(std::forward<Params>(params)...));
    }
    static void Flush();
    static void DumpBacktrace();
    static std::shared_ptr<spdlog::logger> s_AppLogger;
    static spdlog::level::level_enum s_Level;
};

#define __FILENAME__            (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOG_I(...)           ::AppLog::GetLogger()->info(::AppLog::Format(__FILENAME__, __LINE__, __func__, __VA_ARGS__))
#define LOG_W(...)           ::AppLog::GetLogger()->warn(::AppLog::Format(__FILENAME__, __LINE__, __func__, __VA_ARGS__))
#define LOG_E(...)          ::AppLog::GetLogger()->error(::AppLog::Format(__FILENAME__, __LINE__, __func__, __VA_ARGS__))
#define LOG_D(...)          ::AppLog::GetLogger()->debug(::AppLog::Format(__FILENAME__, __LINE__, __func__, __VA_ARGS__))
#define LOG_C(...)           ::AppLog::GetLogger()->critical(::AppLog::Format(__FILENAME__, __LINE__, __func__, __VA_ARGS__))
#define LOG_T(...)          ::AppLog::GetLogger()->trace(::AppLog::Format(__FILENAME__, __LINE__, __func__, __VA_ARGS__))
#define APPLOG_DUMP_BACKTRACE() ::AppLog::DumpBacktrace()