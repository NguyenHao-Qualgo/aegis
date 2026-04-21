#include <gtest/gtest.h>
#include "aegis/installer/packer.hpp"
#include "aegis/common/util.hpp"
#include "aegis/common/error.hpp"
#include <filesystem>

using namespace aegis;

class PackerTest : public ::testing::Test {
protected:
    const std::string sw_desc   = "/tmp/aegis_unittest_swdesc_12345";
    const std::string payload   = "/tmp/aegis_unittest_payload_12345.ext4";
    const std::string sig_file  = "/tmp/aegis_unittest_swdesc_sig_12345";
    const std::string output    = "/tmp/aegis_unittest_output_12345.swu";

    void SetUp() override {
        writeFile(sw_desc,  "software = { version = \"1.0\"; }");
        writeFile(payload,  "fake payload content for testing");
    }

    void TearDown() override {
        for (const auto& p : {sw_desc, payload, sig_file, output})
            std::filesystem::remove(p);
    }
};

TEST_F(PackerTest, PackCreatesOutputFile) {
    PackOptions opts;
    opts.sw_description = sw_desc;
    opts.output_path    = output;

    EXPECT_EQ(Packer(opts).pack(), 0);
    EXPECT_TRUE(std::filesystem::exists(output));
    EXPECT_GT(std::filesystem::file_size(output), 0u);
}

TEST_F(PackerTest, PackWithPayloadSucceeds) {
    PackOptions opts;
    opts.sw_description = sw_desc;
    opts.output_path    = output;
    opts.payloads       = {payload};

    EXPECT_EQ(Packer(opts).pack(), 0);
    EXPECT_TRUE(std::filesystem::exists(output));
}

TEST_F(PackerTest, PackWithSigSucceeds) {
    writeFile(sig_file, "fake RSA signature bytes");
    PackOptions opts;
    opts.sw_description     = sw_desc;
    opts.sw_description_sig = sig_file;
    opts.output_path        = output;

    EXPECT_EQ(Packer(opts).pack(), 0);
    EXPECT_GT(std::filesystem::file_size(output), 0u);
}

TEST_F(PackerTest, PackWithSigAndPayload) {
    writeFile(sig_file, "fake sig");
    PackOptions opts;
    opts.sw_description     = sw_desc;
    opts.sw_description_sig = sig_file;
    opts.output_path        = output;
    opts.payloads           = {payload};

    EXPECT_EQ(Packer(opts).pack(), 0);
}

TEST_F(PackerTest, ReservedNameSwDescriptionThrows) {
    const std::string reserved = "/tmp/sw-description";
    writeFile(reserved, "data");

    PackOptions opts;
    opts.sw_description = sw_desc;
    opts.output_path    = output;
    opts.payloads       = {reserved};

    EXPECT_THROW(Packer(opts).pack(), Error);
    std::filesystem::remove(reserved);
}

TEST_F(PackerTest, ReservedNameSwDescriptionSigThrows) {
    const std::string reserved = "/tmp/sw-description.sig";
    writeFile(reserved, "data");

    PackOptions opts;
    opts.sw_description = sw_desc;
    opts.output_path    = output;
    opts.payloads       = {reserved};

    EXPECT_THROW(Packer(opts).pack(), Error);
    std::filesystem::remove(reserved);
}

TEST_F(PackerTest, InvalidOutputPathThrows) {
    PackOptions opts;
    opts.sw_description = sw_desc;
    opts.output_path    = "/tmp/nonexistent_dir_abc123/out.swu";

    EXPECT_THROW(Packer(opts).pack(), Error);
}

TEST_F(PackerTest, OutputIsSizeMultipleOf512) {
    PackOptions opts;
    opts.sw_description = sw_desc;
    opts.output_path    = output;
    opts.payloads       = {payload};

    Packer(opts).pack();
    const auto size = std::filesystem::file_size(output);
    EXPECT_EQ(size % 512, 0u) << "CPIO archive must be padded to 512-byte blocks";
}
