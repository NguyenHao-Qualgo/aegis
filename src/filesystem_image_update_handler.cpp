#include "aegis/filesystem_image_update_handler.h"
#include "aegis/utils.h"

namespace aegis {

Result<void> FilesystemImageUpdateHandler::install(const std::string& image_path,
                                                   const ManifestImage& image,
                                                   Slot& target_slot,
                                                   ProgressCallback progress) {
    LOG_INFO("Writing filesystem image %s -> %s", image.filename.c_str(),
             target_slot.device.c_str());
    return write_image_to_device(image_path, target_slot.device, progress);
}

} // namespace aegis
