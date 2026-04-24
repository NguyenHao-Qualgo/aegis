#include <gtest/gtest.h>
#include "aegis/common/util.hpp"
#include "aegis/common/logging.hpp"
#include <filesystem>

using namespace aegis;

TEST(StartsWithTest, MatchingPrefix) {
    EXPECT_TRUE(startsWith("hello world", "hello"));
    EXPECT_TRUE(startsWith("hello", "hello"));
}

TEST(StartsWithTest, NonMatchingPrefix) {
    EXPECT_FALSE(startsWith("hello", "world"));
    EXPECT_FALSE(startsWith("", "a"));
    EXPECT_FALSE(startsWith("hi", "hello"));
}

TEST(StartsWithTest, EmptyPrefix) {
    EXPECT_TRUE(startsWith("anything", ""));
    EXPECT_TRUE(startsWith("", ""));
}

TEST(TrimTest, LeadingAndTrailing) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\nhello\t\n"), "hello");
}

TEST(TrimTest, NoWhitespace) {
    EXPECT_EQ(trim("hello"), "hello");
}

TEST(TrimTest, EmptyAndAllWhitespace) {
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("   "), "");
}

TEST(SplitTest, BasicSplit) {
    const auto parts = split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(SplitTest, SingleElement) {
    const auto parts = split("hello", ',');
    ASSERT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "hello");
}

TEST(SplitTest, EmptyInput) {
    const auto parts = split("", ',');
    // std::getline on an empty stream returns 0 elements
    EXPECT_TRUE(parts.empty());
}

TEST(JoinPathTest, Basic) {
    EXPECT_EQ(joinPath("/foo", "bar"), "/foo/bar");
    EXPECT_EQ(joinPath("/foo/", "bar"), "/foo/bar");
}

TEST(FileTest, NonExistentFile) {
    EXPECT_FALSE(fileExists("/tmp/aegis_unittest_nonexistent_xyz_12345"));
}

TEST(FileTest, WriteReadRoundTrip) {
    const std::string path = "/tmp/aegis_unittest_rw_12345.txt";
    writeFile(path, "hello world");
    EXPECT_TRUE(fileExists(path));
    EXPECT_EQ(readFile(path), "hello world");
    std::filesystem::remove(path);
}

TEST(FileTest, WriteCreatesParentDirs) {
    const std::string path = "/tmp/aegis_unittest_subdir_12345/data.txt";
    writeFile(path, "data");
    EXPECT_TRUE(fileExists(path));
    std::filesystem::remove_all("/tmp/aegis_unittest_subdir_12345");
}

TEST(FileTest, ReadNonExistentThrows) {
    EXPECT_THROW(readFile("/tmp/aegis_unittest_missing_xyz.txt"), std::runtime_error);
}

TEST(ShellQuoteTest, PlainString) {
    EXPECT_EQ(shellQuote("hello"), "'hello'");
}

TEST(ShellQuoteTest, StringWithSingleQuote) {
    EXPECT_EQ(shellQuote("it's"), "'it'\\''s'");
}

TEST(ShellQuoteTest, EmptyString) {
    EXPECT_EQ(shellQuote(""), "''");
}

TEST(OptionsTest, GetOptionValueFound) {
    std::vector<std::string> args = {"--config", "/etc/foo.conf", "--verbose"};
    EXPECT_EQ(getOptionValue(args, "--config"), "/etc/foo.conf");
}

TEST(OptionsTest, GetOptionValueNotFound) {
    std::vector<std::string> args = {"--verbose"};
    EXPECT_EQ(getOptionValue(args, "--config"), "");
}

TEST(OptionsTest, GetOptionValueAtEnd) {
    // option at last position has no value
    std::vector<std::string> args = {"--config"};
    EXPECT_EQ(getOptionValue(args, "--config"), "");
}

TEST(OptionsTest, HasOptionTrue) {
    std::vector<std::string> args = {"--verbose", "--config"};
    EXPECT_TRUE(hasOption(args, "--verbose"));
}

TEST(OptionsTest, HasOptionFalse) {
    std::vector<std::string> args = {"--config"};
    EXPECT_FALSE(hasOption(args, "--verbose"));
}

TEST(TimestampTest, NotEmpty) {
    EXPECT_FALSE(currentTimestamp().empty());
}

TEST(TimestampTest, ContainsT) {
    // ISO 8601 format like "2024-01-01T00:00:00Z"
    EXPECT_NE(currentTimestamp().find('T'), std::string::npos);
}

TEST(QuoteTest, StripDoubleQuotes) {
    EXPECT_EQ(strip_quotes("\"hello\""), "hello");
    EXPECT_EQ(strip_quotes("  \"hello\"  "), "hello");
}

TEST(QuoteTest, NoQuotesUnchanged) {
    EXPECT_EQ(strip_quotes("hello"), "hello");
}

TEST(QuoteTest, EmptyQuotedString) {
    EXPECT_EQ(strip_quotes("\"\""), "");
}
