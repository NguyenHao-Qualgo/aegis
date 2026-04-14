#include "aegis/raw_update_handler.h"
#include "aegis/checksum.h"
#include "aegis/utils.h"

#include <cstdio>
#include <unistd.h>

namespace aegis {

Result<void> write_image_to_device(const std::string& source_path,
                                   const std::string& device_path,
                                   ProgressCallback progress) {
    FILE* src = fopen(source_path.c_str(), "rb");
    if (!src)
        return Result<void>::err("Cannot open source: " + source_path);

    FILE* dst = fopen(device_path.c_str(), "wb");
    if (!dst) {
        fclose(src);
        return Result<void>::err("Cannot open device: " + device_path);
    }

    fseek(src, 0, SEEK_END);
    uint64_t total_size = ftell(src);
    fseek(src, 0, SEEK_SET);

    const size_t block_size = 4096 * 64;
    std::vector<uint8_t> buf(block_size);
    uint64_t written = 0;

    while (true) {
        size_t rd = fread(buf.data(), 1, block_size, src);
        if (rd == 0)
            break;

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

    if (!image.sha256.empty()) {
        if (!verify_checksum(image_path, image.sha256, image.size)) {
            return Result<void>::err("Checksum verification failed for " + image.filename);
        }
        LOG_INFO("Checksum verified for %s", image.filename.c_str());
    }

    return write_image_to_device(image_path, target_slot.device, progress);
}

} // namespace aegis
