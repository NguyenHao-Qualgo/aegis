#include "aegis/installer/archive_update_handler.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/mount.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "aegis/bundle/bundle_extractor.hpp"
#include "aegis/command_runner.hpp"
#include "aegis/util.hpp"

namespace aegis {

namespace {

std::string sha256OfFile(const std::string& path) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize SHA-256 context");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Cannot open file for SHA-256: " + path);
    }

    char buf[65536];
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        EVP_DigestUpdate(ctx, buf, static_cast<std::size_t>(file.gcount()));
    }

    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < len; ++i) {
        hex << std::setw(2) << static_cast<int>(digest[i]);
    }
    return hex.str();
}

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

        std::filesystem::create_directories(std::filesystem::path(targetPath).parent_path());

        // If this entry is a hardlink, ensure the target was already extracted;
        // otherwise clear the hardlink so libarchive writes a regular empty file
        // (the data blocks below will fill it in).
        if (const char* linkTarget = archive_entry_hardlink(entry)) {
            const auto absTarget = mountPoint / sanitizeArchivePath(linkTarget);
            if (!std::filesystem::exists(absTarget)) {
                archive_entry_set_hardlink(entry, nullptr);
            }
        }

        const auto headerRc = archive_write_header(writer, entry);
        if (headerRc == ARCHIVE_FATAL) {
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

    ::umount(mountPoint.c_str());

    logInfo("Formatting " + slot.device + " as ext4");
    CommandRunner runner;
    runner.runOrThrow("mkfs.ext4 -F " + slot.device);

    logInfo("Mounting " + slot.device + " -> " + mountPoint.string());
    if (::mount(slot.device.c_str(), mountPoint.c_str(), "ext4", 0, nullptr) != 0) {
        throw std::runtime_error("Failed to mount target slot: " + slot.device);
    }

    try {
        logInfo("Extracting rootfs archive: " + payloadPath);
        extractArchive(payloadPath, mountPoint);

        logInfo("Syncing filesystem");
        ::sync();

        logInfo("Unmounting " + slot.device);
        if (::umount(mountPoint.c_str()) != 0) {
            throw std::runtime_error("Failed to unmount target slot");
        }
    } catch (...) {
        ::umount(mountPoint.c_str());
        std::filesystem::remove_all(workDir);
        throw;
    }
}

void ArchiveUpdateHandler::installFromBundle(const std::string& bundlePath,
                                              std::uint64_t bundlePayloadSize,
                                              const std::string& entryName,
                                              const std::string& expectedSha256,
                                              const SlotConfig& slot,
                                              const std::string& workDir) const {
    BundleExtractor extractor;
    extractor.extractEntry(bundlePath, bundlePayloadSize, entryName, workDir);

    const auto payloadPath = (std::filesystem::path(workDir) /
                              std::filesystem::path(entryName).filename()).string();

    if (!expectedSha256.empty()) {
        logInfo("Verifying SHA-256 of " + payloadPath);
        const auto computed = sha256OfFile(payloadPath);
        if (computed != expectedSha256) {
            std::filesystem::remove_all(workDir);
            throw std::runtime_error("SHA256 mismatch for '" + entryName +
                                     "': expected " + expectedSha256 + ", got " + computed);
        }
    }

    install(payloadPath, slot, workDir);
}

}  // namespace aegis
