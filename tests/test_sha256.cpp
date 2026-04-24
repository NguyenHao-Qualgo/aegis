#include <gtest/gtest.h>
#include "aegis/common/sha256.hpp"
#include <string>

using namespace aegis;

TEST(Sha256Test, EmptyInput) {
    Sha256 h;
    EXPECT_EQ(h.final_hex(),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, AbcInput) {
    Sha256 h;
    const std::string s = "abc";
    h.update(s.c_str(), s.size());
    EXPECT_EQ(h.final_hex(),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, QuickBrownFox) {
    Sha256 h;
    const std::string s = "The quick brown fox jumps over the lazy dog";
    h.update(s.c_str(), s.size());
    EXPECT_EQ(h.final_hex(),
              "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST(Sha256Test, OutputLength) {
    Sha256 h;
    EXPECT_EQ(h.final_hex().size(), 64u);
}

TEST(Sha256Test, IncrementalUpdate) {
    // Feeding "abc" as two separate updates should equal one update of "abc"
    Sha256 h1;
    h1.update("a", 1);
    h1.update("bc", 2);
    const auto hash1 = h1.final_hex();

    Sha256 h2;
    const std::string abc = "abc";
    h2.update(abc.c_str(), abc.size());
    const auto hash2 = h2.final_hex();

    EXPECT_EQ(hash1, hash2);
}

TEST(Sha256Test, ResetClearsState) {
    Sha256 h;
    h.update("abc", 3);
    h.reset();
    // After reset, should produce the empty-string hash
    EXPECT_EQ(h.final_hex(),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, UnsignedCharOverload) {
    Sha256 h1, h2;
    const std::string s = "hello";
    h1.update(s.c_str(), s.size());
    h2.update(reinterpret_cast<const unsigned char*>(s.c_str()), s.size());
    EXPECT_EQ(h1.final_hex(), h2.final_hex());
}

TEST(Sha256Test, MultiBlockInput) {
    // 129 bytes > one SHA-256 block (64 bytes) forces transform twice
    Sha256 h;
    std::string s(129, 'x');
    h.update(s.c_str(), s.size());
    EXPECT_EQ(h.final_hex().size(), 64u);
}

TEST(Sha256Test, TwoDifferentInputsDiffer) {
    Sha256 h1, h2;
    h1.update("hello", 5);
    h2.update("world", 5);
    EXPECT_NE(h1.final_hex(), h2.final_hex());
}
