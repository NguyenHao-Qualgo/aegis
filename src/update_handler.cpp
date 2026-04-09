#include "rauc/update_handler.h"
#include "rauc/checksum.h"
#include "rauc/mount.h"
#include "rauc/utils.h"

#include <cstdio>
#include <cstdio>
#include <unistd.h>

namespace rauc {

Result<void> write_image_to_device(const std::string& source_path,
                                   const std::string& device_path,
                                   ProgressCallback progress) {
    FILE* src = fopen(source_path.c_str(), "rb");
    if (!src) return Result<void>::err("Cannot open source: " + source_path);

    FILE* dst = fopen(device_path.c_str(), "wb");
    if (!dst) {
        fclose(src);
        return Result<void>::err("Cannot open device: " + device_path);
    }

    // Get source file size for progress
    fseek(src, 0, SEEK_END);
    uint64_t total_size = ftell(src);
    fseek(src, 0, SEEK_SET);

    const size_t block_size = 4096 * 64; // 256K blocks
    std::vector<uint8_t> buf(block_size);
    uint64_t written = 0;

    while (true) {
        size_t rd = fread(buf.data(), 1, block_size, src);
        if (rd == 0) break;

        size_t wr = fwrite(buf.data(), 1, rd, dst);
        if (wr != rd) {
            fclose(src);
            fclose(dst);
            return Result<void>::err("Write error at offset " + std::to_string(written));
        }

        written += rd;
        if (progress && total_size > 0) {
            int pct = static_cast<int>(written * 100 / total_size);
            progress(pct, "Writing " + std::to_string(written) + " / " +
                     std::to_string(total_size));
        }
    }

    // Sync
    fflush(dst);
    fsync(fileno(dst));

    fclose(src);
    fclose(dst);

    LOG_INFO("Wrote %lu bytes to %s", written, device_path.c_str());
    return Result<void>::ok();
}

Result<void> RawUpdateHandler::install(const std::string& image_path,
                                       const ManifestImage& image,
                                       Slot& target_slot,
                                       ProgressCallback progress) {
    LOG_INFO("Installing raw image %s -> %s", image.filename.c_str(),
             target_slot.device.c_str());

    // Verify checksum before writing
    if (!image.sha256.empty()) {
        if (!verify_checksum(image_path, image.sha256, image.size)) {
            return Result<void>::err("Checksum verification failed for " + image.filename);
        }
        LOG_INFO("Checksum verified for %s", image.filename.c_str());
    }

    return write_image_to_device(image_path, target_slot.device, progress);
}

Result<void> FileCopyUpdateHandler::install(const std::string& image_path,
                                            const ManifestImage& image,
                                            Slot& target_slot,
                                            ProgressCallback progress) {
    LOG_INFO("Installing filesystem image %s -> %s", image.filename.c_str(),
             target_slot.device.c_str());

    // Format the target partition first
    std::string fstype = to_string(target_slot.type);
    std::vector<std::string> mkfs_cmd;

    if (target_slot.type == SlotType::Ext4) {
        mkfs_cmd = {"mkfs.ext4", "-F", target_slot.device};
    } else if (target_slot.type == SlotType::Vfat) {
        mkfs_cmd = {"mkfs.vfat", target_slot.device};
    }

    if (!mkfs_cmd.empty()) {
        auto res = run_command(mkfs_cmd);
        if (res.exit_code != 0)
            return Result<void>::err("mkfs failed: " + res.stderr_str);
    }

    // Write the image directly to the device
    return write_image_to_device(image_path, target_slot.device, progress);
}

Result<void> TarUpdateHandler::install(const std::string& image_path,
                                       const ManifestImage& image,
                                       Slot& target_slot,
                                       ProgressCallback progress) {
    LOG_INFO("Extracting tar %s -> %s", image.filename.c_str(),
             target_slot.device.c_str());

    // Mount target slot
    std::string mp = "/mnt/rauc/slot-" + target_slot.name;
    auto mount_res = mount(target_slot.device, mp, to_string(target_slot.type));
    if (!mount_res) return Result<void>::err("Cannot mount target: " + mount_res.error());

    // Extract tar
    auto res = run_command({"tar", "xf", image_path, "-C", mp});
    auto umount_res = umount(mp);

    if (res.exit_code != 0)
        return Result<void>::err("tar extraction failed: " + res.stderr_str);
    if (!umount_res)
        return Result<void>::err("umount target failed: " + umount_res.error());

    return Result<void>::ok();
}

std::unique_ptr<IUpdateHandler> create_update_handler(SlotType type, bool is_tar) {
    if (is_tar) return std::make_unique<TarUpdateHandler>();

    switch (type) {
        case SlotType::Raw:
        case SlotType::Nand:
        case SlotType::Nor:
        case SlotType::BootEmmc:
        case SlotType::BootMbrSwitch:
        case SlotType::BootGptSwitch:
        case SlotType::BootRawFallback:
            return std::make_unique<RawUpdateHandler>();

        case SlotType::Ext4:
        case SlotType::Vfat:
        case SlotType::Ubifs:
        case SlotType::Ubivol:
        case SlotType::Jffs2:
            return std::make_unique<FileCopyUpdateHandler>();
    }
    return std::make_unique<RawUpdateHandler>();
}

} // namespace rauc
