#include <gtest/gtest.h>
#include "aegis/io/command_runner.hpp"
#include <stdexcept>

using namespace aegis;

TEST(CommandRunnerTest, RunEchoSuccess) {
    CommandRunner runner;
    const auto result = runner.run("echo hello");
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_NE(result.output.find("hello"), std::string::npos);
}

TEST(CommandRunnerTest, RunFalseFailure) {
    CommandRunner runner;
    const auto result = runner.run("false");
    EXPECT_NE(result.exitCode, 0);
}

TEST(CommandRunnerTest, RunTrueSuccess) {
    CommandRunner runner;
    const auto result = runner.run("true");
    EXPECT_EQ(result.exitCode, 0);
}

TEST(CommandRunnerTest, RunCapturesOutput) {
    CommandRunner runner;
    const auto result = runner.run("echo 'test output'");
    EXPECT_NE(result.output.find("test output"), std::string::npos);
}

TEST(CommandRunnerTest, RunMultiLineOutput) {
    CommandRunner runner;
    const auto result = runner.run("printf 'line1\\nline2\\nline3\\n'");
    EXPECT_NE(result.output.find("line1"), std::string::npos);
    EXPECT_NE(result.output.find("line3"), std::string::npos);
}

TEST(CommandRunnerTest, RunOrThrowSuccess) {
    CommandRunner runner;
    const auto output = runner.runOrThrow("echo hello");
    EXPECT_NE(output.find("hello"), std::string::npos);
}

TEST(CommandRunnerTest, RunOrThrowFailureThrows) {
    CommandRunner runner;
    EXPECT_THROW(runner.runOrThrow("false"), std::runtime_error);
}

TEST(CommandRunnerTest, RunOrThrowExceptionContainsCommand) {
    CommandRunner runner;
    try {
        runner.runOrThrow("false");
        FAIL() << "Expected exception";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("false"), std::string::npos);
    }
}

TEST(CommandRunnerTest, RunOrThrowNonZeroExit) {
    CommandRunner runner;
    EXPECT_THROW(runner.runOrThrow("exit 42"), std::runtime_error);
}
