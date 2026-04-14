#include "aegis/update_handler.h"
#include "aegis/archive_update_handler.h"
#include "aegis/filesystem_image_update_handler.h"
#include "aegis/raw_update_handler.h"

namespace aegis {

std::unique_ptr<IUpdateHandler> create_update_handler(SlotType type, bool is_tar) {
    if (is_tar)
        return std::make_unique<ArchiveUpdateHandler>();

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
            return std::make_unique<FilesystemImageUpdateHandler>();
    }

    return std::make_unique<RawUpdateHandler>();
}

} // namespace aegis
