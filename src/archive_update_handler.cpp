#include "aegis/archive_update_handler.h"
#include "aegis/context.h"
#include "aegis/mount.h"
#include "aegis/utils.h"

#include <archive.h>
#include <archive_entry.h>
#include <cerrno>
#include <cstring>
#include <limits.h>
#include <unistd.h>

namespace aegis {

namespace {

Result<void> copy_archive_data(struct archive* reader,
                               struct archive* writer,
                               const char* entry_name) {
    while (true) {
        const void* buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;

        int rc = archive_read_data_block(reader, &buffer, &size, &offset);
        if (rc == ARCHIVE_EOF)
            return Result<void>::ok();
        if (rc != ARCHIVE_OK) {
            return Result<void>::err("archive_read_data_block failed for '" +
                                     std::string(entry_name ? entry_name : "<unknown>") +
                                     "': " + archive_error_string(reader));
        }

        rc = archive_write_data_block(writer, buffer, size, offset);
        if (rc != ARCHIVE_OK) {
            return Result<void>::err("archive_write_data_block failed for '" +
                                     std::string(entry_name ? entry_name : "<unknown>") +
                                     "': " + archive_error_string(writer));
        }
    }
}

Result<void> extract_archive_to_directory(const std::string& archive_path,
                                          const std::string& destination) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        return Result<void>::err("Failed to determine current working directory: " +
                                 std::string(std::strerror(errno)));
    }

    if (::chdir(destination.c_str()) != 0) {
        return Result<void>::err("Failed to enter extraction directory '" + destination +
                                 "': " + std::string(std::strerror(errno)));
    }

    struct CwdGuard {
        explicit CwdGuard(std::string path) : path_(std::move(path)) {}
        ~CwdGuard() { ::chdir(path_.c_str()); }
        std::string path_;
    } cwd_guard(cwd);

    struct ArchiveReader {
        archive* value = archive_read_new();
        ~ArchiveReader() {
            if (value) {
                archive_read_close(value);
                archive_read_free(value);
            }
        }
    } reader;

    struct ArchiveWriter {
        archive* value = archive_write_disk_new();
        ~ArchiveWriter() {
            if (value)
                archive_write_free(value);
        }
    } writer;

    if (!reader.value || !writer.value)
        return Result<void>::err("Failed to allocate libarchive objects");

    archive_read_support_format_tar(reader.value);
    archive_read_support_filter_all(reader.value);
    archive_write_disk_set_options(
        writer.value,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS |
        ARCHIVE_EXTRACT_XATTR |
        ARCHIVE_EXTRACT_SECURE_NODOTDOT |
        ARCHIVE_EXTRACT_SECURE_SYMLINKS |
        ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS);
    archive_write_disk_set_standard_lookup(writer.value);

    int rc = archive_read_open_filename(reader.value, archive_path.c_str(), 10240);
    if (rc != ARCHIVE_OK) {
        return Result<void>::err("archive_read_open_filename failed: " +
                                 std::string(archive_error_string(reader.value)));
    }

    while (true) {
        archive_entry* entry = nullptr;
        rc = archive_read_next_header(reader.value, &entry);
        if (rc == ARCHIVE_EOF)
            break;
        if (rc != ARCHIVE_OK) {
            return Result<void>::err("archive_read_next_header failed: " +
                                     std::string(archive_error_string(reader.value)));
        }

        const char* entry_name = archive_entry_pathname(entry);
        rc = archive_write_header(writer.value, entry);
        if (rc != ARCHIVE_OK) {
            return Result<void>::err("archive_write_header failed for '" +
                                     std::string(entry_name ? entry_name : "<unknown>") +
                                     "': " + archive_error_string(writer.value));
        }

        auto copy_res = copy_archive_data(reader.value, writer.value, entry_name);
        if (!copy_res)
            return copy_res;

        rc = archive_write_finish_entry(writer.value);
        if (rc != ARCHIVE_OK) {
            return Result<void>::err("archive_write_finish_entry failed for '" +
                                     std::string(entry_name ? entry_name : "<unknown>") +
                                     "': " + archive_error_string(writer.value));
        }
    }

    return Result<void>::ok();
}

} // namespace

Result<void> ArchiveUpdateHandler::install(const std::string& image_path,
                                           const ManifestImage& image,
                                           Slot& target_slot,
                                           ProgressCallback /*progress*/) {
    LOG_INFO("Extracting tar %s -> %s", image.filename.c_str(),
             target_slot.device.c_str());

    std::string mount_point = create_mount_point(Context::instance().mount_prefix(),
                                                 "slot-" + target_slot.name);
    auto mount_res = mount(target_slot.device, mount_point, to_string(target_slot.type));
    if (!mount_res)
        return Result<void>::err("Cannot mount target: " + mount_res.error());

    auto extract_res = extract_archive_to_directory(image_path, mount_point);
    auto umount_res = umount(mount_point);

    if (!extract_res)
        return Result<void>::err("archive extraction failed: " + extract_res.error());
    if (!umount_res)
        return Result<void>::err("umount target failed: " + umount_res.error());

    return Result<void>::ok();
}

} // namespace aegis
