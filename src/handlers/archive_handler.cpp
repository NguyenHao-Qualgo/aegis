#include "aegis/handlers/archive_handler.h"
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

int clamp_progress(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

void emit_progress(const ProgressCallback& progress, int percent, const std::string& message) {
    if (progress) {
        progress(clamp_progress(percent), message);
    }
}

Result<void> copy_archive_data(struct archive* reader,
                               struct archive* writer,
                               const char* entry_name,
                               uint64_t archive_size,
                               const ProgressCallback& progress,
                               int start_percent,
                               int end_percent) {
    int last_percent = -1;

    while (true) {
        const void* buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;

        int rc = archive_read_data_block(reader, &buffer, &size, &offset);
        if (rc == ARCHIVE_EOF) {
            emit_progress(progress,
                          end_percent,
                          "Extracted " + std::string(entry_name ? entry_name : "<unknown>"));
            return Result<void>::ok();
        }

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

        if (progress && archive_size > 0) {
            la_int64_t consumed = archive_filter_bytes(reader, 0);
            if (consumed < 0) {
                consumed = 0;
            }

            int mapped =
                start_percent +
                static_cast<int>((static_cast<uint64_t>(consumed) * (end_percent - start_percent)) /
                                 archive_size);

            if (mapped != last_percent) {
                emit_progress(progress,
                              mapped,
                              "Extracting " +
                                  std::string(entry_name ? entry_name : "<unknown>"));
                last_percent = mapped;
            }
        }
    }
}

Result<void> extract_archive_to_directory(const std::string& archive_path,
                                          const std::string& destination,
                                          const ProgressCallback& progress) {
    const uint64_t archive_size = file_size(archive_path);

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
        ~CwdGuard() {
            ::chdir(path_.c_str());
        }
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

    if (!reader.value || !writer.value) {
        return Result<void>::err("Failed to allocate libarchive objects");
    }

    archive_read_support_format_tar(reader.value);
    archive_read_support_filter_all(reader.value);
    archive_write_disk_set_options(
        writer.value,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
            ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR |
            ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS |
            ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS);
    archive_write_disk_set_standard_lookup(writer.value);

    emit_progress(progress, 15, "Opening archive");

    int rc = archive_read_open_filename(reader.value, archive_path.c_str(), 10240);
    if (rc != ARCHIVE_OK) {
        return Result<void>::err("archive_read_open_filename failed: " +
                                 std::string(archive_error_string(reader.value)));
    }

    emit_progress(progress, 20, "Extracting archive");

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

        auto copy_res =
            copy_archive_data(reader.value, writer.value, entry_name, archive_size, progress, 20, 85);
        if (!copy_res) {
            return copy_res;
        }

        rc = archive_write_finish_entry(writer.value);
        if (rc != ARCHIVE_OK) {
            return Result<void>::err("archive_write_finish_entry failed for '" +
                                     std::string(entry_name ? entry_name : "<unknown>") +
                                     "': " + archive_error_string(writer.value));
        }
    }

    emit_progress(progress, 85, "Archive extraction finished");
    return Result<void>::ok();
}

} // namespace

Result<void> MountedArchiveUpdateHandler::install(const std::string& image_path,
                                                  const ManifestImage& image,
                                                  Slot& target_slot,
                                                  ProgressCallback progress) {
    LOG_INFO("Extracting tar %s -> %s", image.filename.c_str(), target_slot.device.c_str());

    emit_progress(progress, 5, "Preparing target mount");

    std::string mount_point =
        create_mount_point(Context::instance().mount_prefix(), "slot-" + target_slot.name);

    auto mount_res = mount(target_slot.device, mount_point, to_string(target_slot.type));
    if (!mount_res) {
        return Result<void>::err("Cannot mount target: " + mount_res.error());
    }

    emit_progress(progress, 10, "Mounted target filesystem");

    auto extract_res = extract_archive_to_directory(image_path, mount_point, progress);

    emit_progress(progress, 90, "Syncing filesystem");
    ::sync();

    auto umount_res = umount(mount_point);

    if (!extract_res) {
        return Result<void>::err("archive extraction failed: " + extract_res.error());
    }
    if (!umount_res) {
        return Result<void>::err("umount target failed: " + umount_res.error());
    }

    emit_progress(progress, 100, "Archive installation completed");
    return Result<void>::ok();
}

} // namespace aegis