#include "aegis/handlers/archive_handler.h"
#include "aegis/context.h"
#include "aegis/mount.h"
#include "aegis/utils.h"

#include <archive.h>
#include <archive_entry.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace aegis {

namespace {

constexpr size_t kPipeBufferSize = 1024 * 1024;
constexpr size_t kArchiveReadBlockSize = 4096;

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

struct ExtractThreadData {
    int read_fd = -1;
    std::string destination;
    int flags = 0;
    int exit_code = -1;
    std::string error_message;
};

Result<void> copy_archive_data(struct archive* reader, struct archive* writer) {
    while (true) {
        const void* buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;

        int rc = archive_read_data_block(reader, &buffer, &size, &offset);
        if (rc == ARCHIVE_EOF) {
            return Result<void>::ok();
        }
        if (rc != ARCHIVE_OK) {
            return Result<void>::err(std::string("archive_read_data_block failed: ") +
                                     archive_error_string(reader));
        }

        rc = archive_write_data_block(writer, buffer, size, offset);
        if (rc != ARCHIVE_OK) {
            return Result<void>::err(std::string("archive_write_data_block failed: ") +
                                     archive_error_string(writer));
        }
    }
}

void* extract_thread_main(void* userdata) {
    auto* data = static_cast<ExtractThreadData*>(userdata);
    data->exit_code = -1;
    data->error_message.clear();

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        data->error_message =
            "Failed to determine current working directory: " + std::string(std::strerror(errno));
        pthread_exit(nullptr);
    }

    if (::chdir(data->destination.c_str()) != 0) {
        data->error_message =
            "Failed to enter extraction directory '" + data->destination + "': " +
            std::string(std::strerror(errno));
        pthread_exit(nullptr);
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
            if (value) {
                archive_write_free(value);
            }
        }
    } writer;

    if (!reader.value || !writer.value) {
        data->error_message = "Failed to allocate libarchive objects";
        pthread_exit(nullptr);
    }

    archive_read_support_format_all(reader.value);
    archive_read_support_filter_all(reader.value);

    archive_write_disk_set_options(writer.value, data->flags);
    archive_write_disk_set_standard_lookup(writer.value);

    int rc = archive_read_open_fd(reader.value, data->read_fd, kArchiveReadBlockSize);
    if (rc != ARCHIVE_OK) {
        data->error_message =
            "archive_read_open_fd failed: " + std::string(archive_error_string(reader.value));
        pthread_exit(nullptr);
    }

    while (true) {
        archive_entry* entry = nullptr;
        rc = archive_read_next_header(reader.value, &entry);
        if (rc == ARCHIVE_EOF) {
            break;
        }
        if (rc != ARCHIVE_OK) {
            data->error_message =
                "archive_read_next_header failed: " + std::string(archive_error_string(reader.value));
            pthread_exit(nullptr);
        }

        rc = archive_write_header(writer.value, entry);
        if (rc != ARCHIVE_OK) {
            data->error_message =
                "archive_write_header failed: " + std::string(archive_error_string(writer.value));
            pthread_exit(nullptr);
        }

        auto copy_res = copy_archive_data(reader.value, writer.value);
        if (!copy_res) {
            data->error_message = copy_res.error();
            pthread_exit(nullptr);
        }

        rc = archive_write_finish_entry(writer.value);
        if (rc != ARCHIVE_OK) {
            data->error_message =
                "archive_write_finish_entry failed: " +
                std::string(archive_error_string(writer.value));
            pthread_exit(nullptr);
        }
    }

    data->exit_code = 0;
    pthread_exit(nullptr);
}

Result<void> stream_archive_to_pipe(const std::string& archive_path,
                                    int write_fd,
                                    const ProgressCallback& progress) {
    int src_fd = ::open(archive_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (src_fd < 0) {
        return Result<void>::err("Cannot open archive: " + archive_path + ": " +
                                 std::string(std::strerror(errno)));
    }

    const uint64_t total_size = file_size(archive_path);
    uint64_t copied = 0;
    int last_percent = -1;

    std::vector<uint8_t> buffer(kPipeBufferSize);

    emit_progress(progress, 15, "Opening archive");
    emit_progress(progress, 20, "Extracting archive");

    while (true) {
        ssize_t rd = ::read(src_fd, buffer.data(), buffer.size());
        if (rd == 0) {
            break;
        }
        if (rd < 0) {
            int saved = errno;
            ::close(src_fd);
            return Result<void>::err("Read error from archive: " + archive_path + ": " +
                                     std::string(std::strerror(saved)));
        }

        uint8_t* out_ptr = buffer.data();
        ssize_t remaining = rd;

        while (remaining > 0) {
            ssize_t wr = ::write(write_fd, out_ptr, static_cast<size_t>(remaining));
            if (wr < 0) {
                int saved = errno;
                ::close(src_fd);
                return Result<void>::err("Write to archive pipe failed: " +
                                         std::string(std::strerror(saved)));
            }

            remaining -= wr;
            out_ptr += wr;
            copied += static_cast<uint64_t>(wr);
        }

        if (progress && total_size > 0) {
            int mapped = 20 + static_cast<int>((copied * 65) / total_size); // 20..85
            mapped = clamp_progress(mapped);

            if (mapped != last_percent) {
                emit_progress(progress, mapped, "Extracting archive");
                last_percent = mapped;
            }
        }
    }

    ::close(src_fd);
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

    int pipefd[2] = {-1, -1};
    if (::pipe(pipefd) != 0) {
        auto umount_res = umount(mount_point);
        (void)umount_res;
        return Result<void>::err("pipe() failed: " + std::string(std::strerror(errno)));
    }

    ExtractThreadData thread_data;
    thread_data.read_fd = pipefd[0];
    thread_data.destination = mount_point;
    thread_data.flags =
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR |
        ARCHIVE_EXTRACT_SECURE_NODOTDOT | ARCHIVE_EXTRACT_SECURE_SYMLINKS |
        ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;

    pthread_t thread {};
    int thread_create_rc = pthread_create(&thread, nullptr, extract_thread_main, &thread_data);
    if (thread_create_rc != 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        auto umount_res = umount(mount_point);
        (void)umount_res;
        return Result<void>::err("pthread_create failed: " +
                                 std::string(std::strerror(thread_create_rc)));
    }

    auto stream_res = stream_archive_to_pipe(image_path, pipefd[1], progress);
    ::close(pipefd[1]);
    pipefd[1] = -1;

    void* join_status = nullptr;
    int join_rc = pthread_join(thread, &join_status);
    ::close(pipefd[0]);
    pipefd[0] = -1;

    emit_progress(progress, 90, "Syncing filesystem");
    ::sync();

    auto umount_res = umount(mount_point);

    if (!stream_res) {
        return Result<void>::err("archive stream failed: " + stream_res.error());
    }
    if (join_rc != 0) {
        return Result<void>::err("pthread_join failed: " +
                                 std::string(std::strerror(join_rc)));
    }
    if (thread_data.exit_code != 0) {
        return Result<void>::err("archive extraction failed: " + thread_data.error_message);
    }
    if (!umount_res) {
        return Result<void>::err("umount target failed: " + umount_res.error());
    }

    emit_progress(progress, 100, "Archive installation completed");
    return Result<void>::ok();
}

} // namespace aegis