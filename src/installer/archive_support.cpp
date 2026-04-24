#include "aegis/installer/archive_support.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <locale.h>
#include <pthread.h>
#include <sys/mount.h>
#include <thread>
#include <unistd.h>

#include "aegis/common/error.hpp"
#include "aegis/common/logging.hpp"

namespace aegis {

namespace {

constexpr std::size_t kArchiveReadBlockSize = 1024 * 1024;
int debug = 0;

int copy_data(struct archive* ar, struct archive* aw, struct archive_entry*) {
    int r;
    const void* buff;
    size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t offset;
#else
    off_t offset;
#endif

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) {
            return ARCHIVE_OK;
        }
        if (r != ARCHIVE_OK) {
            if (r == ARCHIVE_WARN) {
                continue;
            }
            return r;
        }
        r = static_cast<int>(archive_write_data_block(aw, buff, size, offset));
        if (r != ARCHIVE_OK) {
            return r;
        }
    }
}

}  // namespace

int archive_extract_flags(const bool preserve_attributes) {
    if (!preserve_attributes) {
        return 0;
    }

    return ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM |
           ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL |
           ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR;
}

RestoreCwd::~RestoreCwd() {
    if (active) {
        ::chdir(old_cwd.c_str());
    }
}

JoinThread::~JoinThread() {
    if (thread != nullptr && thread->joinable()) {
        thread->join();
    }
}

UnlinkPath::~UnlinkPath() {
    if (!path.empty()) {
        ::unlink(path.c_str());
    }
}

RemoveTree::~RemoveTree() {
    if (!path.empty()) {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
}

ScopedMount::~ScopedMount() {
    if (mounted) {
        ::sync();
        if (::umount(mountpoint.c_str()) != 0) {
            LOG_E("failed to unmount '" + mountpoint.string() + "'");
        }
    }
}

void extract_archive_to_disk(ExtractData* data) {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    locale_t archive_locale = newlocale(LC_CTYPE_MASK, "", static_cast<locale_t>(0));
    locale_t old_locale = static_cast<locale_t>(0);
    if (archive_locale != static_cast<locale_t>(0)) {
        old_locale = uselocale(archive_locale);
    }
#endif

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    struct archive_entry* entry = nullptr;
    int exitval = -EFAULT;

    if (!a || !ext) {
        data->error_detail = "libarchive allocation failed";
        goto out;
    }

    archive_write_disk_set_options(ext, data->flags);
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, data->fifo_path.c_str(), kArchiveReadBlockSize) != ARCHIVE_OK) {
        data->error_detail = std::string("archive_read_open_filename failed: ") +
                             (archive_error_string(a) ? archive_error_string(a) : "unknown");
        goto out;
    }

    LOG_I("archive handler: libarchive opened FIFO '" + data->fifo_path +
          "' and is consuming streamed payload data");

    for (;;) {
        const int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r != ARCHIVE_OK) {
            if (r == ARCHIVE_WARN) {
                continue;
            }
            data->error_detail = std::string("archive_read_next_header failed: ") +
                                 (archive_error_string(a) ? archive_error_string(a) : "unknown");
            goto out;
        }

        if (debug != 0) {
            (void)archive_entry_pathname(entry);
        }

        ++data->extracted_entries;
        if (archive_write_header(ext, entry) != ARCHIVE_OK) {
            data->error_detail = std::string("archive_write_header failed: ") +
                                 (archive_error_string(ext) ? archive_error_string(ext) : "unknown");
            goto out;
        }
        if (copy_data(a, ext, entry) != ARCHIVE_OK) {
            data->error_detail = std::string("archive data copy failed: ") +
                                 (archive_error_string(a) ? archive_error_string(a) : "unknown");
            goto out;
        }
        if (archive_write_finish_entry(ext) != ARCHIVE_OK) {
            data->error_detail = std::string("archive_write_finish_entry failed: ") +
                                 (archive_error_string(ext) ? archive_error_string(ext) : "unknown");
            goto out;
        }
    }

    if (data->extracted_entries == 0) {
        data->error_detail =
            "archive payload contained no extractable entries; check manifest type/compression";
        goto out;
    }

    exitval = 0;

out:
    if (ext) {
        if (archive_write_free(ext) != ARCHIVE_OK) {
            if (data->error_detail.empty()) {
                data->error_detail = "archive_write_free failed";
            }
            exitval = -EFAULT;
        }
    }
    if (a) {
        archive_read_close(a);
        archive_read_free(a);
    }

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    if (archive_locale != static_cast<locale_t>(0)) {
        uselocale(old_locale);
        freelocale(archive_locale);
    }
#endif
    data->exitval = exitval;
}

fs::path make_target_extract_path(const fs::path& mountpoint, const std::string& entry_path) {
    if (entry_path.empty()) {
        return mountpoint;
    }

    fs::path rel = fs::path(entry_path).is_absolute()
        ? fs::path(entry_path).relative_path()
        : fs::path(entry_path);

    return mountpoint / rel;
}

void mount_target_device(const ManifestEntry& entry, const fs::path& mountpoint) {
    if (entry.device.empty()) {
        fail_runtime("archive handler missing device for " + entry.filename);
    }
    if (entry.filesystem.empty()) {
        fail_runtime("archive handler missing filesystem for " + entry.filename);
    }

    if (::mount(entry.device.c_str(),
                mountpoint.c_str(),
                entry.filesystem.c_str(),
                0,
                nullptr) != 0) {
        fail_runtime("failed to mount '" + entry.device + "' on '" + mountpoint.string() + "'");
    }
}

}  // namespace aegis
