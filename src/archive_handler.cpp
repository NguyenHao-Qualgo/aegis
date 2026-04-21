#include "aegis/handlers.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <filesystem>
#include <locale.h>
#include <pthread.h>
#include <sys/stat.h>
#include <thread>

#include "aegis/io.hpp"
#include "aegis/logging.hpp"
#include "aegis/payload.hpp"
#include "aegis/types.hpp"

namespace aegis {

namespace {

constexpr char kFifoName[] = "archivfifo";
int debug = 0;

struct RestoreCwd {
    std::string old_cwd;
    bool active = false;

    ~RestoreCwd() {
        if (active) {
            ::chdir(old_cwd.c_str());
        }
    }
};

struct JoinThread {
    std::thread *thread = nullptr;

    ~JoinThread() {
        if (thread != nullptr && thread->joinable()) {
            thread->join();
        }
    }
};

struct UnlinkPath {
    std::string path;

    ~UnlinkPath() {
        if (!path.empty()) {
            ::unlink(path.c_str());
        }
    }
};

struct ExtractData {
    int flags = 0;
    int exitval = -EFAULT;
    std::string fifo_path;
    std::string error_detail;
};

int copy_data(struct archive *ar, struct archive *aw, struct archive_entry * /*entry*/) {
    int r;
    const void *buff;
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

void extract(ExtractData *data) {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
    locale_t archive_locale = newlocale(LC_CTYPE_MASK, "", static_cast<locale_t>(0));
    locale_t old_locale = static_cast<locale_t>(0);
    if (archive_locale != static_cast<locale_t>(0)) {
        old_locale = uselocale(archive_locale);
    }
#endif

    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    struct archive_entry *entry = nullptr;
    int exitval = -EFAULT;

    if (!a || !ext) {
        data->error_detail = "libarchive allocation failed";
        goto out;
    }

    archive_write_disk_set_options(ext, data->flags);
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, data->fifo_path.c_str(), 4096) != ARCHIVE_OK) {
        data->error_detail = std::string("archive_read_open_filename failed: ") +
                             (archive_error_string(a) ? archive_error_string(a) : "unknown");
        goto out;
    }

    log_stream("archive handler: libarchive opened FIFO '" + data->fifo_path +
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

void install_archive_image(StreamReader &reader,
                           const CpioEntry &cpio_entry,
                           const ManifestEntry &entry,
                           const AesMaterial *aes) {
    if (entry.path.empty()) {
        fail_runtime("archive handler missing path for " + entry.filename);
    }

    fs::path target = entry.path;
    log_stream("archive handler: target path='" + target.string() +
               "', extraction via FIFO + libarchive");
    std::error_code ec;
    if (entry.create_destination) {
        fs::create_directories(target, ec);
        if (ec) {
            fail_runtime("cannot create destination " + target.string());
        }
    }

    char pwd[PATH_MAX] = "\0";
    if (::getcwd(pwd, sizeof(pwd)) == nullptr) {
        fail_runtime("failed to determine current working directory");
    }
    RestoreCwd restore{pwd, true};

    if (::chdir(target.c_str()) != 0) {
        fail_runtime("chdir failed for " + target.string());
    }

    fs::path fifo = fs::temp_directory_path() / kFifoName;
    ::unlink(fifo.c_str());
    if (::mkfifo(fifo.c_str(), 0600) != 0) {
        fail_runtime("FIFO cannot be created in archive handler");
    }
    UnlinkPath fifo_guard{fifo.string()};
    log_stream("archive handler: created FIFO '" + fifo.string() +
               "' for streamed archive extraction");

    ExtractData extract_data{};
    extract_data.fifo_path = fifo.string();
    if (entry.preserve_attributes) {
        extract_data.flags |= ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM |
                              ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL |
                              ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR;
    }

    std::thread extractor([&]() { extract(&extract_data); });
    JoinThread extractor_guard{&extractor};
    log_stream("archive handler: started extraction thread");

    FileDescriptor fdout(::open(fifo.c_str(), O_WRONLY));
    if (!fdout) {
        fail_runtime("failed to open FIFO " + fifo.string());
    }

    auto sink = [&](const char *data, std::size_t len) {
        write_all_fd(fdout.get(), data, len);
    };

    log_stream("archive handler: writing payload bytes from SWU stream into FIFO");

    if (entry.encrypted) {
        if (!aes) {
            fail_runtime("encrypted payload requires --aes-key");
        }
        stream_encrypted_payload(reader, cpio_entry, *aes, entry.ivt, sink, entry.sha256);
    } else {
        stream_plain_payload(reader, cpio_entry, sink, entry.sha256);
    }

    fdout.reset();
    if (extractor.joinable()) {
        extractor.join();
    }

    if (extract_data.exitval != 0) {
        std::string message = "archive extraction failed for " + entry.filename;
        if (!extract_data.error_detail.empty()) {
            message += "\n" + extract_data.error_detail;
        }
        fail_runtime(message);
    }

    ::sync();
    log_stream("archive handler: completed streamed extraction into '" + target.string() + "'");
}

}  // namespace

void ArchiveHandler::install(StreamReader &reader,
                             const CpioEntry &cpio_entry,
                             const ManifestEntry &entry,
                             const AesMaterial *aes) {
    install_archive_image(reader, cpio_entry, entry, aes);
}

}  // namespace aegis
