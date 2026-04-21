#include <gtest/gtest.h>
#include "aegis/crypto/crypto.hpp"
#include "aegis/common/error.hpp"
#include <filesystem>
#include <fstream>

using namespace aegis;

TEST(HexToBytesTest, BasicLowercase) {
    const auto b = hex_to_bytes("deadbeef");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0xde);
    EXPECT_EQ(b[1], 0xad);
    EXPECT_EQ(b[2], 0xbe);
    EXPECT_EQ(b[3], 0xef);
}

TEST(HexToBytesTest, BasicUppercase) {
    const auto b = hex_to_bytes("DEADBEEF");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0xde);
    EXPECT_EQ(b[3], 0xef);
}

TEST(HexToBytesTest, MixedCase) {
    const auto b = hex_to_bytes("DeAdBeEf");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0xde);
}

TEST(HexToBytesTest, EmptyInput) {
    EXPECT_TRUE(hex_to_bytes("").empty());
}

TEST(HexToBytesTest, ZeroBytes) {
    const auto b = hex_to_bytes("0000");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x00);
    EXPECT_EQ(b[1], 0x00);
}

TEST(HexToBytesTest, AllFF) {
    const auto b = hex_to_bytes("ffff");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0xff);
    EXPECT_EQ(b[1], 0xff);
}

TEST(HexToBytesTest, OddLengthThrows) {
    EXPECT_THROW(hex_to_bytes("abc"), Error);
    EXPECT_THROW(hex_to_bytes("a"),   Error);
}

TEST(HexToBytesTest, InvalidCharThrows) {
    EXPECT_THROW(hex_to_bytes("gg"), Error);
    EXPECT_THROW(hex_to_bytes("zz"), Error);
    EXPECT_THROW(hex_to_bytes("0x"), Error);
}

TEST(EvpCipherTest, Aes128) {
    EXPECT_NE(evp_cipher_for_key_length(16), nullptr);
}

TEST(EvpCipherTest, Aes192) {
    EXPECT_NE(evp_cipher_for_key_length(24), nullptr);
}

TEST(EvpCipherTest, Aes256) {
    EXPECT_NE(evp_cipher_for_key_length(32), nullptr);
}

TEST(EvpCipherTest, InvalidLengthReturnsNull) {
    EXPECT_EQ(evp_cipher_for_key_length(0),  nullptr);
    EXPECT_EQ(evp_cipher_for_key_length(10), nullptr);
    EXPECT_EQ(evp_cipher_for_key_length(17), nullptr);
    EXPECT_EQ(evp_cipher_for_key_length(64), nullptr);
}

TEST(CollectOpenSSLErrorsTest, NoErrorsReturnsEmpty) {
    // This relies on no prior errors being queued; acceptable in test context
    EXPECT_NO_THROW(collect_openssl_errors());
}

TEST(LoadPublicKeyTest, MissingFileSetsDetail) {
    std::string detail;
    EVP_PKEY* key = load_public_key_or_certificate(
        "/tmp/aegis_unittest_noexist_key.pem", detail);
    EXPECT_EQ(key, nullptr);
    EXPECT_FALSE(detail.empty());
}

TEST(LoadPublicKeyTest, InvalidPemFileSetsDetail) {
    const std::string path = "/tmp/aegis_unittest_badkey_12345.pem";
    std::ofstream{path} << "this is not a PEM key\n";
    std::string detail;
    EVP_PKEY* key = load_public_key_or_certificate(path, detail);
    EXPECT_EQ(key, nullptr);
    EXPECT_FALSE(detail.empty());
    std::filesystem::remove(path);
}
