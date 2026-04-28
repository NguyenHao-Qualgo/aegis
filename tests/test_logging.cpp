#include <gtest/gtest.h>

#include "aegis/common/logging.hpp"

TEST(AppLogTest, InitUsesConfiguredLoggerAndSinkLevels) {
    AppLog::Init(AppLog::Level::info, nullptr, "test");

    ASSERT_NE(AppLog::GetLogger(), nullptr);
    EXPECT_EQ(AppLog::GetLogger()->level(), spdlog::level::info);
    for (const auto& sink : AppLog::GetLogger()->sinks()) {
        EXPECT_EQ(sink->level(), spdlog::level::info);
    }
}

TEST(AppLogTest, ReinitUpdatesLoggerLevel) {
    AppLog::Init(AppLog::Level::debug, nullptr, "test");
    EXPECT_EQ(AppLog::GetLogger()->level(), spdlog::level::debug);

    AppLog::Init(AppLog::Level::warn, nullptr, "test");
    EXPECT_EQ(AppLog::GetLogger()->level(), spdlog::level::warn);
    for (const auto& sink : AppLog::GetLogger()->sinks()) {
        EXPECT_EQ(sink->level(), spdlog::level::warn);
    }
}
