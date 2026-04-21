#include <gtest/gtest.h>
#include "aegis/config/config.hpp"
#include "aegis/common/util.hpp"
#include <filesystem>

using namespace aegis;

class ConfigTest : public ::testing::Test {
protected:
    const std::string path{"/tmp/aegis_unittest_config_12345.conf"};

    void TearDown() override { std::filesystem::remove(path); }

    void write(const std::string& content) { writeFile(path, content); }
};

TEST_F(ConfigTest, LoadsAllKeys) {
    write("public-key = /path/key.pem\n"
          "aes-key = /path/aes.key\n"
          "data-directory = /var/lib/aegis\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key,      "/path/key.pem");
    EXPECT_EQ(cfg.aes_key,         "/path/aes.key");
    EXPECT_EQ(cfg.data_directory,  "/var/lib/aegis");
}

TEST_F(ConfigTest, LoadsFromUpdateSection) {
    write("[update]\npublic-key = /key.pem\ndata-directory = /data\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key,     "/key.pem");
    EXPECT_EQ(cfg.data_directory, "/data");
}

TEST_F(ConfigTest, IgnoresUnknownSection) {
    write("[other]\npublic-key = /wrong.pem\n"
          "[update]\npublic-key = /correct.pem\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/correct.pem");
}

TEST_F(ConfigTest, SkipsHashComments) {
    write("# comment line\npublic-key = /key.pem\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/key.pem");
}

TEST_F(ConfigTest, SkipsSemicolonComments) {
    write("; another comment\npublic-key = /key.pem\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/key.pem");
}

TEST_F(ConfigTest, SkipsEmptyLines) {
    write("\n\npublic-key = /key.pem\n\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/key.pem");
}

TEST_F(ConfigTest, StripDoubleQuotes) {
    write("public-key = \"/path/to/key.pem\"\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/path/to/key.pem");
}

TEST_F(ConfigTest, StripSingleQuotes) {
    write("public-key = '/path/to/key.pem'\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/path/to/key.pem");
}

TEST_F(ConfigTest, MissingFileThrows) {
    EXPECT_THROW(ConfigLoader{}.load("/tmp/aegis_noexist_conf_xyz.conf"), std::runtime_error);
}

TEST_F(ConfigTest, MissingEqualsThrows) {
    write("public-key /path/key.pem\n");
    EXPECT_THROW(ConfigLoader{}.load(path), std::runtime_error);
}

TEST_F(ConfigTest, EmptyValueAllowed) {
    write("aes-key =\n");
    EXPECT_NO_THROW(ConfigLoader{}.load(path));
}

TEST_F(ConfigTest, WindowsLineEndingsTrimmed) {
    write("public-key = /key.pem\r\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/key.pem");
}

TEST_F(ConfigTest, UnknownKeysIgnored) {
    write("unknown-setting = something\npublic-key = /key.pem\n");
    const auto cfg = ConfigLoader{}.load(path);
    EXPECT_EQ(cfg.public_key, "/key.pem");
}
