#include "aegis/handlers/update_handler.h"
#include "aegis/handlers/archive_handler.h"
#include "aegis/handlers/fs_image_handler.h"
#include "aegis/handlers/raw_handler.h"

#include <array>

namespace aegis {

namespace {

template <size_t N>
bool contains_slot_type(const std::array<SlotType, N>& supported_types, SlotType slot_type) {
    for (SlotType supported_type : supported_types) {
        if (supported_type == slot_type)
            return true;
    }
    return false;
}

constexpr std::array kRawLikeSlotTypes = {
    SlotType::Raw,
    SlotType::Nand,
    SlotType::Nor,
    SlotType::BootEmmc,
    SlotType::BootMbrSwitch,
    SlotType::BootGptSwitch,
    SlotType::BootRawFallback,
};

constexpr std::array kFilesystemImageSlotTypes = {
    SlotType::Ext4,
    SlotType::Vfat,
    SlotType::Ubifs,
    SlotType::Ubivol,
    SlotType::Jffs2,
};

bool has_suffix(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

UpdatePayloadKind UpdateHandlerFactory::classify_payload(const std::string& filename) {
    if (has_suffix(filename, ".tar") ||
        has_suffix(filename, ".tar.gz") ||
        has_suffix(filename, ".tgz") ||
        has_suffix(filename, ".tar.xz") ||
        has_suffix(filename, ".txz") ||
        has_suffix(filename, ".tar.bz2") ||
        has_suffix(filename, ".tbz2")) {
        return UpdatePayloadKind::Archive;
    }

    return UpdatePayloadKind::BlockImage;
}

std::unique_ptr<UpdateHandler> UpdateHandlerFactory::create(SlotType slot_type,
                                                            UpdatePayloadKind payload_kind) {
    if (payload_kind == UpdatePayloadKind::Archive)
        return std::make_unique<MountedArchiveUpdateHandler>();

    if (contains_slot_type(kRawLikeSlotTypes, slot_type))
        return std::make_unique<RawSlotUpdateHandler>();

    if (contains_slot_type(kFilesystemImageSlotTypes, slot_type))
        return std::make_unique<FilesystemImageUpdateHandler>();

    return std::make_unique<RawSlotUpdateHandler>();
}

} // namespace aegis
