#include "aegis/handlers/update_handler.h"
#include "aegis/handlers/archive_handler.h"
#include "aegis/handlers/fs_image_handler.h"
#include "aegis/handlers/raw_handler.h"

#include <array>

namespace aegis {

namespace {

bool has_suffix(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool uses_filesystem_image_handler(SlotType slot_type) {
    switch (slot_type) {
    case SlotType::Ext4:
        return true;
    default:
        return false;
    }
}

} // namespace



UpdatePayloadKind UpdateHandlerFactory::classify_payload(const std::string& filename) {
    if (has_suffix(filename, ".tar") || has_suffix(filename, ".tar.gz") ||
        has_suffix(filename, ".tgz") || has_suffix(filename, ".tar.xz") ||
        has_suffix(filename, ".txz") || has_suffix(filename, ".tar.bz2") ||
        has_suffix(filename, ".tbz2")) {
        return UpdatePayloadKind::Archive;
    }

    return UpdatePayloadKind::BlockImage;
}

std::unique_ptr<UpdateHandler> UpdateHandlerFactory::create(SlotType slot_type,
                                                            UpdatePayloadKind payload_kind) {
    if (payload_kind == UpdatePayloadKind::Archive)
        return std::make_unique<MountedArchiveUpdateHandler>();

    if (uses_filesystem_image_handler(slot_type))
        return std::make_unique<FilesystemImageUpdateHandler>();

    return std::make_unique<RawSlotUpdateHandler>();
}

} // namespace aegis
