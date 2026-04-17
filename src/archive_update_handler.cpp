#include "aegis/archive_update_handler.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <stdexcept>
#include <sys/mount.h>
#include <unistd.h>

namespace aegis {

namespace {

std::filesystem::path sanitizeArchivePath(const std::string& path) {
    auto relative = std::filesystem::path(path).lexically_normal();
    if (relative.empty()) {
        throw std::runtime_error("Archive entry has empty path");
    }
    if (relative.is_absolute()) {
        relative = relative.relative_path();
    }
    for (const auto& part : relative) {
        if (part == "..") {
            throw std::runtime_error("Archive entry escapes target root: " + path);
        }
    }
    return relative;
}

void clearMountedDirectory(const std::filesystem::path& mountPoint) {
    if (!std::filesystem::exists(mountPoint)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(mountPoint)) {
        std::filesystem::remove_all(entry.path());
    }
}

void extractArchive(const std::string& archivePath, const std::filesystem::path& mountPoint) {
    archive* reader = archive_read_new();
    archive* writer = archive_write_disk_new();
    if (reader == nullptr || writer == nullptr) {
        if (reader != nullptr) archive_read_free(reader);
        if (writer != nullptr) archive_write_free(writer);
        throw std::runtime_error("Failed to initialize libarchive");
    }

    archive_read_support_format_all(reader);
    archive_read_support_filter_all(reader);

    archive_write_disk_set_options(writer,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS |
        ARCHIVE_EXTRACT_OWNER |
        ARCHIVE_EXTRACT_XATTR);
    archive_write_disk_set_standard_lookup(writer);

    if (archive_read_open_filename(reader, archivePath.c_str(), 10240) != ARCHIVE_OK) {
        const auto message = std::string("Cannot open archive payload: ") + archive_error_string(reader);
        archive_write_free(writer);
        archive_read_free(reader);
        throw std::runtime_error(message);
    }

    archive_entry* entry = nullptr;
    while (true) {
        const auto rc = archive_read_next_header(reader, &entry);
        if (rc == ARCHIVE_EOF) {
            break;
        }
        if (rc != ARCHIVE_OK) {
            const auto message = std::string("Failed reading archive header: ") + archive_error_string(reader);
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_free(writer);
            throw std::runtime_error(message);
        }

        const auto originalPath = archive_entry_pathname(entry);
        const auto sanitizedPath = sanitizeArchivePath(originalPath == nullptr ? std::string{} : std::string(originalPath));
        const auto targetPath = (mountPoint / sanitizedPath).string();
        archive_entry_set_pathname(entry, targetPath.c_str());

        if (archive_write_header(writer, entry) != ARCHIVE_OK) {
            const auto message = std::string("Failed writing archive entry: ") + archive_error_string(writer);
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error(message);
        }

        const void* buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while (true) {
            const auto dataRc = archive_read_data_block(reader, &buffer, &size, &offset);
            if (dataRc == ARCHIVE_EOF) {
                break;
            }
            if (dataRc != ARCHIVE_OK) {
                const auto message = std::string("Failed reading archive data: ") + archive_error_string(reader);
                archive_read_close(reader);
                archive_read_free(reader);
                archive_write_close(writer);
                archive_write_free(writer);
                throw std::runtime_error(message);
            }
            if (archive_write_data_block(writer, buffer, size, offset) != ARCHIVE_OK) {
                const auto message = std::string("Failed writing archive data: ") + archive_error_string(writer);
                archive_read_close(reader);
                archive_read_free(reader);
                archive_write_close(writer);
                archive_write_free(writer);
                throw std::runtime_error(message);
            }
        }

        if (archive_write_finish_entry(writer) != ARCHIVE_OK) {
            const auto message = std::string("Failed finishing archive entry: ") + archive_error_string(writer);
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error(message);
        }
    }

    archive_read_close(reader);
    archive_read_free(reader);
    archive_write_close(writer);
    archive_write_free(writer);
}

}  // namespace

bool ArchiveUpdateHandler::supportsImageType(const std::string& imageType) const {
    return imageType == "archive";
}

void ArchiveUpdateHandler::install(const std::string& payloadPath, const SlotConfig& slot, const std::string& workDir) const {
    if (slot.type != SlotType::Ext4) {
        throw std::runtime_error("Archive handler only supports ext4 slots");
    }

    const auto mountPoint = std::filesystem::path(workDir) / "mnt";
    std::filesystem::create_directories(mountPoint);
    if (::mount(slot.device.c_str(), mountPoint.c_str(), "ext4", 0, nullptr) != 0) {
        throw std::runtime_error("Failed to mount target slot: " + slot.device);
    }

    try {
        clearMountedDirectory(mountPoint);
        extractArchive(payloadPath, mountPoint);
        ::sync();
        if (::umount(mountPoint.c_str()) != 0) {
            throw std::runtime_error("Failed to unmount target slot");
        }
    } catch (...) {
        ::umount(mountPoint.c_str());
        throw;
    }
}

}  // namespace aegis
