#include <gtest/gtest.h>
#include "aegis/core/types.hpp"
#include "aegis/common/error.hpp"

using namespace aegis;

TEST(TypesToStringTest, BootloaderType) {
    EXPECT_EQ(toString(BootloaderType::UBoot), "uboot");
    EXPECT_EQ(toString(BootloaderType::Nvidia), "nvidia");
}

TEST(TypesToStringTest, SlotType) {
    EXPECT_EQ(toString(SlotType::Ext4), "ext4");
}

TEST(TypesToStringTest, OtaStateAllValues) {
    EXPECT_EQ(toString(OtaState::Idle),     "Idle");
    EXPECT_EQ(toString(OtaState::Download), "Download");
    EXPECT_EQ(toString(OtaState::Install),  "Install");
    EXPECT_EQ(toString(OtaState::Reboot),   "Reboot");
    EXPECT_EQ(toString(OtaState::Commit),   "Commit");
    EXPECT_EQ(toString(OtaState::Failure),  "Failure");
}

TEST(TypesParseTest, BootloaderTypeValidValues) {
    EXPECT_EQ(parseBootloaderType("uboot"),  BootloaderType::UBoot);
    EXPECT_EQ(parseBootloaderType("nvidia"), BootloaderType::Nvidia);
}

TEST(TypesParseTest, BootloaderTypeInvalidThrows) {
    EXPECT_THROW(parseBootloaderType("grub"),    std::runtime_error);
    EXPECT_THROW(parseBootloaderType(""),        std::runtime_error);
    EXPECT_THROW(parseBootloaderType("UBOOT"),   std::runtime_error);
}

TEST(TypesParseTest, SlotTypeValid) {
    EXPECT_EQ(parseSlotType("ext4"), SlotType::Ext4);
}

TEST(TypesParseTest, SlotTypeInvalidThrows) {
    EXPECT_THROW(parseSlotType("btrfs"),  std::runtime_error);
    EXPECT_THROW(parseSlotType(""),       std::runtime_error);
    EXPECT_THROW(parseSlotType("EXT4"),   std::runtime_error);
}

TEST(TypesConstantsTest, IoBufferSize) {
    EXPECT_EQ(kIoBufferSize, 64u * 1024u);
}

TEST(TypesConstantsTest, CpioHeaderSize) {
    EXPECT_EQ(kCpioHeaderSize, 110u);
}

TEST(TypesConstantsTest, TrailerName) {
    EXPECT_STREQ(kTrailerName, "TRAILER!!!");
}

TEST(OtaStatusTest, DefaultValues) {
    OtaStatus s;
    EXPECT_EQ(s.state, OtaState::Idle);
    EXPECT_EQ(s.progress, 0);
    EXPECT_EQ(s.bootedSlot, "A");
    EXPECT_EQ(s.primarySlot, "A");
    EXPECT_FALSE(s.targetSlot.has_value());
}
