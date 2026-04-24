#include <gtest/gtest.h>
#include "aegis/installer/manifest.hpp"
#include <filesystem>
#include <fstream>

using namespace aegis;

// Minimal sw-description with one raw image
static const std::string kBasicManifest = R"(
software =
{
    version = "1.0";
    images: (
        {
            filename = "rootfs.ext4";
            type = "raw";
            device = "/dev/mmcblk0p2";
            sha256 = "aabbccdd";
        }
    );
}
)";

static const std::string kMultiManifest = R"(
software =
{
    images: (
        {
            filename = "rootfs.ext4";
            type = "raw";
            device = "/dev/sda1";
        },
        {
            filename = "overlay.tar.gz";
            type = "archive";
            path = "/opt";
        },
        {
            filename = "data.tar";
            type = "tar";
            path = "/data";
        }
    );
}
)";

static const std::string kImagesAndFilesManifest = R"(
software =
{
    images: (
        {
            filename = "rootfs.ext4.gz";
            type = "raw";
            device = "/dev/sda1";
            compress = "zlib";
        }
    );
    files: (
        {
            filename = "esp.tar.gz";
            type = "archive";
            path = "/boot/efi";
        },
        {
            filename = "secondLoader.enc";
            type = "file";
            path = "/boot/efi/EFI/BOOT/secondLoader.enc";
        }
    );
}
)";

static const std::string kSlottedManifest = R"(
software =
{
    version = "1.0.0";

    B: {
        images: (
            {
                filename = "rootfs-b.ext4";
                type = "raw";
                device = "/dev/sdb1";
            }
        );
    };
    A: {
        images: (
            {
                filename = "rootfs-a.ext4";
                type = "raw";
                device = "/dev/sda1";
            }
        );
    };
}
)";

TEST(ManifestParseTest, BasicRawEntry) {
    const auto entries = parse_manifest(kBasicManifest);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].filename, "rootfs.ext4");
    EXPECT_EQ(entries[0].type,     "raw");
    EXPECT_EQ(entries[0].device,   "/dev/mmcblk0p2");
    EXPECT_EQ(entries[0].sha256,   "aabbccdd");
}

TEST(ManifestParseTest, MultipleEntries) {
    const auto entries = parse_manifest(kMultiManifest);
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].type, "raw");
    EXPECT_EQ(entries[1].type, "archive");
    EXPECT_EQ(entries[2].type, "tar");
}

TEST(ManifestParseTest, ImagesAndFilesEntries) {
    const auto entries = parse_manifest(kImagesAndFilesManifest);
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].filename, "rootfs.ext4.gz");
    EXPECT_EQ(entries[0].type, "raw");
    EXPECT_EQ(entries[0].compress, "zlib");
    EXPECT_EQ(entries[1].filename, "esp.tar.gz");
    EXPECT_EQ(entries[1].type, "archive");
    EXPECT_EQ(entries[1].path, "/boot/efi");
    EXPECT_TRUE(entries[1].device.empty());
    EXPECT_TRUE(entries[1].filesystem.empty());
    EXPECT_EQ(entries[2].filename, "secondLoader.enc");
    EXPECT_EQ(entries[2].type, "file");
    EXPECT_EQ(entries[2].path, "/boot/efi/EFI/BOOT/secondLoader.enc");
}

TEST(ManifestParseTest, WithTargetSlot) {
    const auto entries = parse_manifest(kSlottedManifest, "B");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].filename, "rootfs-b.ext4");
    EXPECT_EQ(entries[0].device,   "/dev/sdb1");
}

TEST(ManifestParseTest, SlotAExtraction) {
    const auto entries = parse_manifest(kSlottedManifest, "A");
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].filename, "rootfs-a.ext4");
}

TEST(ManifestParseTest, DefaultTypeIsRaw) {
    const std::string manifest = R"(
        software = { images: ({ filename = "x.ext4"; device = "/dev/sda1"; }); }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].type, "raw");
}

TEST(ManifestParseTest, SkipsUnsupportedType) {
    const std::string manifest = R"(
        software = {
            images: (
                { filename = "bad.swu"; type = "swu"; device = "/dev/sda1"; },
                { filename = "good.ext4"; type = "raw"; device = "/dev/sda1"; }
            );
        }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].filename, "good.ext4");
}

TEST(ManifestParseTest, SkipsEntriesWithoutFilename) {
    const std::string manifest = R"(
        software = {
            images: (
                { type = "raw"; device = "/dev/sda1"; },
                { filename = "good.ext4"; type = "raw"; device = "/dev/sda1"; }
            );
        }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 1u);
}

TEST(ManifestParseTest, EncryptedEntry) {
    const std::string manifest = R"(
        software = {
            images: ({
                filename = "enc.ext4";
                type = "raw";
                device = "/dev/sda1";
                encrypted = true;
                ivt = "0011223344556677";
            });
        }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].encrypted);
    EXPECT_EQ(entries[0].ivt, "0011223344556677");
}

TEST(ManifestParseTest, CompressedEntry) {
    const std::string manifest = R"(
        software = {
            images: ({
                filename = "rootfs.ext4.gz";
                type = "raw";
                device = "/dev/sda1";
                compress = "zlib";
            });
        }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].compress, "zlib");
}

TEST(ManifestParseTest, BoolFields) {
    const std::string manifest = R"(
        software = {
            images: ({
                filename = "x.ext4";
                type = "raw";
                device = "/dev/sda1";
                preserve-attributes = true;
                create-destination = true;
                atomic-install = true;
            });
        }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].preserve_attributes);
    EXPECT_TRUE(entries[0].create_destination);
    EXPECT_TRUE(entries[0].atomic_install);
}

TEST(ManifestParseTest, EmptyImagesThrows) {
    const std::string manifest = "software = { images: (); }";
    EXPECT_THROW(parse_manifest(manifest), Error);
}

TEST(ManifestParseTest, NoImagesBlockThrows) {
    const std::string manifest = "software = { version = \"1.0\"; }";
    EXPECT_THROW(parse_manifest(manifest), Error);
}

TEST(ManifestParseTest, FilesOnlyManifest) {
    const std::string manifest = R"(
        software = {
            files: (
                {
                    filename = "esp.tar.gz";
                    type = "archive";
                    path = "/boot/efi";
                },
                {
                    filename = "secondLoader.enc";
                    type = "file";
                    path = "/boot/efi/EFI/BOOT/secondLoader.enc";
                }
            );
        }
    )";
    const auto entries = parse_manifest(manifest);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].filename, "esp.tar.gz");
    EXPECT_EQ(entries[0].path, "/boot/efi");
    EXPECT_EQ(entries[1].filename, "secondLoader.enc");
    EXPECT_EQ(entries[1].type, "file");
    EXPECT_EQ(entries[1].path, "/boot/efi/EFI/BOOT/secondLoader.enc");
}

TEST(FindManifestEntryTest, EntryFound) {
    std::vector<ManifestEntry> entries;
    ManifestEntry e;
    e.filename = "rootfs.ext4";
    entries.push_back(e);

    const auto* found = find_manifest_entry(entries, "rootfs.ext4");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->filename, "rootfs.ext4");
}

TEST(FindManifestEntryTest, EntryNotFound) {
    std::vector<ManifestEntry> entries;
    EXPECT_EQ(find_manifest_entry(entries, "missing.ext4"), nullptr);
}

TEST(FindManifestEntryTest, ReturnsFirstMatch) {
    std::vector<ManifestEntry> entries;
    ManifestEntry a, b;
    a.filename = "rootfs.ext4";
    a.device   = "/dev/sda1";
    b.filename = "rootfs.ext4";
    b.device   = "/dev/sdb1";
    entries.push_back(a);
    entries.push_back(b);

    const auto* found = find_manifest_entry(entries, "rootfs.ext4");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->device, "/dev/sda1");
}

TEST(ManifestHelpersTest, ToBoolTrue) {
    EXPECT_TRUE(to_bool("true"));
}

TEST(ManifestHelpersTest, ToBoolFalse) {
    EXPECT_FALSE(to_bool("false"));
    EXPECT_FALSE(to_bool(""));
    EXPECT_FALSE(to_bool("True"));
    EXPECT_FALSE(to_bool("1"));
}

TEST(ParseAesKeyFileTest, ValidFile) {
    const std::string path = "/tmp/aegis_unittest_aes_12345.key";
    {
        std::ofstream f(path);
        f << "0102030405060708090a0b0c0d0e0f10 a0b1c2d3e4f5060708090a0b0c0d0e0f\n";
    }
    const auto aes = parse_aes_key_file(path);
    EXPECT_EQ(aes.key_hex, "0102030405060708090a0b0c0d0e0f10");
    EXPECT_EQ(aes.iv_hex,  "a0b1c2d3e4f5060708090a0b0c0d0e0f");
    std::filesystem::remove(path);
}

TEST(ParseAesKeyFileTest, MissingFileThrows) {
    EXPECT_THROW(parse_aes_key_file("/tmp/aegis_unittest_noexist_aes.key"), Error);
}
