#include "aegis/handlers/block_handler.h"
#include "aegis/utils.h"

namespace aegis {

Result<void> BlockDeviceUpdateHandler::install(const std::string& image_path,
                                               const ManifestImage& image, Slot& target_slot,
                                               ProgressCallback progress) {
    LOG_INFO("Installing %s %s -> %s", target_kind(), image.filename.c_str(),
             target_slot.device.c_str());

    if (!image.sha256.empty()) {
        if (!verify_checksum(image_path, image.sha256, image.size)) {
            return Result<void>::err("Checksum verification failed for " + image.filename);
        }
        LOG_INFO("Checksum verified for %s", image.filename.c_str());
    }

    return write_image_to_device(image_path,
                             target_slot.device,
                             image.filename + " to " + target_slot.name,
                             progress);
}

} // namespace aegis
