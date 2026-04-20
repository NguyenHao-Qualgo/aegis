#include "aegis/bundle/bundle_extractor.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <iomanip>
#include <sstream>

#include <archive.h>
#include <archive_entry.h>
#include <openssl/evp.h>

namespace aegis {

namespace {

// RAII wrapper for OpenSSL EVP digest context.
class Sha256Context {
public:
    Sha256Context() : ctx_(EVP_MD_CTX_new()) {
        if (!ctx_ || EVP_DigestInit_ex(ctx_, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx_);
            throw std::runtime_error("Failed to initialize SHA-256 context");
        }
    }
    ~Sha256Context() { EVP_MD_CTX_free(ctx_); }
    Sha256Context(const Sha256Context&) = delete;
    Sha256Context& operator=(const Sha256Context&) = delete;

    void update(const void* data, std::size_t len) {
        EVP_DigestUpdate(ctx_, data, len);
    }

    void updateZeros(std::int64_t count) {
        static constexpr uint8_t zeros[4096] = {};
        while (count > 0) {
            const auto chunk = std::min<std::int64_t>(count, static_cast<std::int64_t>(sizeof(zeros)));
            EVP_DigestUpdate(ctx_, zeros, static_cast<std::size_t>(chunk));
            count -= chunk;
        }
    }

    std::string hexDigest() {
        uint8_t digest[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        EVP_DigestFinal_ex(ctx_, digest, &len);
        std::ostringstream hex;
        hex << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < len; ++i) {
            hex << std::setw(2) << static_cast<int>(digest[i]);
        }
        return hex.str();
    }

private:
    EVP_MD_CTX* ctx_;
};

struct BundlePayloadSource {
    std::ifstream file;
    std::uint64_t remaining{0};
    char buffer[65536];
};

la_ssize_t payloadReadCallback(archive*, void* data, const void** buf) {
    auto* src = static_cast<BundlePayloadSource*>(data);
    if (src->remaining == 0) return 0;
    const auto chunk = static_cast<std::streamsize>(
        std::min<std::uint64_t>(src->remaining, sizeof(src->buffer)));
    src->file.read(src->buffer, chunk);
    const auto got = src->file.gcount();
    src->remaining -= static_cast<std::uint64_t>(got);
    *buf = src->buffer;
    return static_cast<la_ssize_t>(got);
}

archive* openBundleReader(BundlePayloadSource& src, const std::string& bundlePath,
                           std::uint64_t payloadSize) {
    src.file.open(bundlePath, std::ios::binary);
    if (!src.file) throw std::runtime_error("Cannot open bundle: " + bundlePath);
    src.remaining = payloadSize;

    archive* reader = archive_read_new();
    if (!reader) throw std::runtime_error("Failed to initialize libarchive reader");
    archive_read_support_format_tar(reader);
    archive_read_support_filter_gzip(reader);

    if (archive_read_open(reader, &src, nullptr, payloadReadCallback, nullptr) != ARCHIVE_OK) {
        const auto msg = std::string(archive_error_string(reader));
        archive_read_free(reader);
        throw std::runtime_error("Failed to open bundle payload: " + msg);
    }
    return reader;
}

}  // namespace

void BundleExtractor::extract(const std::string& bundlePath, std::uint64_t payloadSize,
                               const std::string& destDir) const {
    BundlePayloadSource src;
    archive* reader = openBundleReader(src, bundlePath, payloadSize);

    archive* writer = archive_write_disk_new();
    if (!writer) {
        archive_read_free(reader);
        throw std::runtime_error("Failed to initialize libarchive writer");
    }
    archive_write_disk_set_options(
        writer, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(writer);

    const std::filesystem::path destPath(destDir);
    archive_entry* entry = nullptr;
    while (true) {
        const auto rc = archive_read_next_header(reader, &entry);
        if (rc == ARCHIVE_EOF) break;
        if (rc != ARCHIVE_OK) {
            const auto msg = std::string(archive_error_string(reader));
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error("Failed reading bundle archive: " + msg);
        }

        const auto entryName = std::filesystem::path(archive_entry_pathname(entry)).lexically_normal();
        if (entryName.is_absolute() || *entryName.begin() == "..") {
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error("Bundle entry escapes extraction directory: " + entryName.string());
        }
        archive_entry_set_pathname(entry, (destPath / entryName).string().c_str());

        if (archive_write_header(writer, entry) != ARCHIVE_OK) {
            const auto msg = std::string(archive_error_string(writer));
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error("Failed writing archive entry: " + msg);
        }

        const void* block;
        std::size_t size;
        std::int64_t offset;
        while (true) {
            const auto dataRc = archive_read_data_block(reader, &block, &size, &offset);
            if (dataRc == ARCHIVE_EOF) break;
            if (dataRc != ARCHIVE_OK) {
                const auto msg = std::string(archive_error_string(reader));
                archive_read_close(reader);
                archive_read_free(reader);
                archive_write_close(writer);
                archive_write_free(writer);
                throw std::runtime_error("Failed reading archive data: " + msg);
            }
            if (archive_write_data_block(writer, block, size, offset) != ARCHIVE_OK) {
                const int err = archive_errno(writer);
                const auto msg = std::string(archive_error_string(writer)) +
                    " (errno " + std::to_string(err) + ": " + std::strerror(err) + ")";
                archive_read_close(reader);
                archive_read_free(reader);
                archive_write_close(writer);
                archive_write_free(writer);
                throw std::runtime_error("Failed writing archive data: " + msg);
            }
        }

        if (archive_write_finish_entry(writer) != ARCHIVE_OK) {
            const auto msg = std::string(archive_error_string(writer));
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error("Failed finishing archive entry: " + msg);
        }
    }

    archive_read_close(reader);
    archive_read_free(reader);
    archive_write_close(writer);
    archive_write_free(writer);
}

void BundleExtractor::extractEntry(const std::string& bundlePath, std::uint64_t payloadSize,
                                    const std::string& entryName,
                                    const std::string& destDir) const {
    BundlePayloadSource src;
    archive* reader = openBundleReader(src, bundlePath, payloadSize);

    archive* writer = archive_write_disk_new();
    if (!writer) {
        archive_read_free(reader);
        throw std::runtime_error("Failed to initialize libarchive writer");
    }
    archive_write_disk_set_options(writer, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM);
    archive_write_disk_set_standard_lookup(writer);

    const std::filesystem::path destPath(destDir);
    const auto targetName = std::filesystem::path(entryName).lexically_normal();
    archive_entry* entry = nullptr;
    bool found = false;
    while (true) {
        const auto rc = archive_read_next_header(reader, &entry);
        if (rc == ARCHIVE_EOF) break;
        if (rc != ARCHIVE_OK) {
            const auto msg = std::string(archive_error_string(reader));
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error("Failed reading bundle archive: " + msg);
        }

        const auto name = std::filesystem::path(archive_entry_pathname(entry)).lexically_normal();
        if (name != targetName) {
            archive_read_data_skip(reader);
            continue;
        }

        found = true;
        archive_entry_set_pathname(entry, (destPath / targetName).string().c_str());
        if (archive_write_header(writer, entry) != ARCHIVE_OK) {
            const auto msg = std::string(archive_error_string(writer));
            archive_read_close(reader);
            archive_read_free(reader);
            archive_write_close(writer);
            archive_write_free(writer);
            throw std::runtime_error("Failed writing entry header: " + msg);
        }

        const void* block;
        std::size_t size;
        std::int64_t offset;
        while (true) {
            const auto dataRc = archive_read_data_block(reader, &block, &size, &offset);
            if (dataRc == ARCHIVE_EOF) break;
            if (dataRc != ARCHIVE_OK) {
                const auto msg = std::string(archive_error_string(reader));
                archive_read_close(reader);
                archive_read_free(reader);
                archive_write_close(writer);
                archive_write_free(writer);
                throw std::runtime_error("Failed reading entry data: " + msg);
            }
            if (archive_write_data_block(writer, block, size, offset) != ARCHIVE_OK) {
                const int err = archive_errno(writer);
                const auto msg = std::string(archive_error_string(writer)) +
                    " (errno " + std::to_string(err) + ": " + std::strerror(err) + ")";
                archive_read_close(reader);
                archive_read_free(reader);
                archive_write_close(writer);
                archive_write_free(writer);
                throw std::runtime_error("Failed writing entry data: " + msg);
            }
        }
        archive_write_finish_entry(writer);
        break;
    }

    archive_read_close(reader);
    archive_read_free(reader);
    archive_write_close(writer);
    archive_write_free(writer);

    if (!found) {
        throw std::runtime_error("Entry '" + entryName + "' not found in bundle");
    }
}

void BundleExtractor::streamEntry(const std::string& bundlePath, std::uint64_t payloadSize,
                                   const std::string& entryName,
                                   const std::string& destPath,
                                   const std::string& expectedSha256) const {
    BundlePayloadSource src;
    archive* reader = openBundleReader(src, bundlePath, payloadSize);

    const int outFd = ::open(destPath.c_str(), O_WRONLY);
    if (outFd < 0) {
        archive_read_close(reader);
        archive_read_free(reader);
        throw std::runtime_error("Cannot open destination '" + destPath + "': " +
                                 std::strerror(errno));
    }

    archive_entry* entry = nullptr;
    bool found = false;

    while (true) {
        const auto rc = archive_read_next_header(reader, &entry);
        if (rc == ARCHIVE_EOF) break;
        if (rc != ARCHIVE_OK) {
            const auto msg = std::string(archive_error_string(reader));
            ::close(outFd);
            archive_read_close(reader);
            archive_read_free(reader);
            throw std::runtime_error("Failed reading bundle archive: " + msg);
        }

        const auto name = std::filesystem::path(archive_entry_pathname(entry)).lexically_normal();
        if (name != std::filesystem::path(entryName).lexically_normal()) {
            archive_read_data_skip(reader);
            continue;
        }

        found = true;
        const std::int64_t entrySize = archive_entry_size(entry);
        Sha256Context sha256ctx;
        std::int64_t currentPos = 0;
        const void* block;
        std::size_t size;
        std::int64_t offset;
        while (true) {
            const auto dataRc = archive_read_data_block(reader, &block, &size, &offset);
            if (dataRc == ARCHIVE_EOF) break;
            if (dataRc != ARCHIVE_OK) {
                const auto msg = std::string(archive_error_string(reader));
                ::close(outFd);
                archive_read_close(reader);
                archive_read_free(reader);
                throw std::runtime_error("Failed reading entry data: " + msg);
            }

            if (offset > currentPos) {
                sha256ctx.updateZeros(offset - currentPos);
            }
            sha256ctx.update(block, size);
            currentPos = offset + static_cast<std::int64_t>(size);

            const auto* data = static_cast<const char*>(block);
            auto remaining = static_cast<ssize_t>(size);
            auto writeOffset = static_cast<off_t>(offset);
            while (remaining > 0) {
                const auto written = ::pwrite(outFd, data, static_cast<std::size_t>(remaining), writeOffset);
                if (written < 0) {
                    const auto err = std::string(std::strerror(errno));
                    ::close(outFd);
                    archive_read_close(reader);
                    archive_read_free(reader);
                    throw std::runtime_error("Failed writing to '" + destPath + "': " + err);
                }
                data += written;
                writeOffset += written;
                remaining -= written;
            }
        }

        if (entrySize > currentPos) {
            sha256ctx.updateZeros(entrySize - currentPos);
        }

        if (!expectedSha256.empty()) {
            const auto computed = sha256ctx.hexDigest();
            if (computed != expectedSha256) {
                ::close(outFd);
                archive_read_close(reader);
                archive_read_free(reader);
                throw std::runtime_error("SHA256 mismatch for '" + entryName +
                                         "': expected " + expectedSha256 + ", got " + computed);
            }
        }
        break;
    }

    ::fsync(outFd);
    ::sync();
    ::close(outFd);
    archive_read_close(reader);
    archive_read_free(reader);

    if (!found) {
        throw std::runtime_error("Entry '" + entryName + "' not found in bundle");
    }
}

}  // namespace aegis
