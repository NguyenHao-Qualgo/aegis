#include <gtest/gtest.h>
#include "aegis/common/error.hpp"
#include <stdexcept>

using namespace aegis;

TEST(ErrorTest, FailRuntimeThrowsError) {
    EXPECT_THROW(fail_runtime("boom"), Error);
}

TEST(ErrorTest, ErrorIsRuntimeError) {
    EXPECT_THROW(fail_runtime("boom"), std::runtime_error);
}

TEST(ErrorTest, ErrorMessagePreserved) {
    try {
        fail_runtime("my specific message");
        FAIL() << "Expected Error to be thrown";
    } catch (const Error& e) {
        EXPECT_STREQ(e.what(), "my specific message");
    }
}

TEST(ErrorTest, CatchAsBaseClass) {
    bool caught = false;
    try {
        fail_runtime("test");
    } catch (const std::exception& e) {
        caught = true;
        EXPECT_STREQ(e.what(), "test");
    }
    EXPECT_TRUE(caught);
}
