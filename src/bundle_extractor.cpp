#include "aegis/bundle_extractor.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <archive.h>
#include <archive_entry.h>

namespace aegis {

namespace {

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

}  // namespace

void BundleExtractor::extract(const std::string& bundlePath, const std::string& destDir,
                               std::uint64_t payloadSize) const {
    BundlePayloadSource src;
    src.file.open(bundlePath, std::ios::binary);
    if (!src.file) {
        throw std::runtime_error("Cannot open bundle: " + bundlePath);
    }
    src.remaining = payloadSize;

    archive* reader = archive_read_new();
    archive* writer = archive_write_disk_new();
    if (!reader || !writer) {
        if (reader) archive_read_free(reader);
        if (writer) archive_write_free(writer);
        throw std::runtime_error("Failed to initialize libarchive");
    }

    archive_read_support_format_tar(reader);
    archive_read_support_filter_gzip(reader);
    archive_write_disk_set_options(
        writer, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_FFLAGS);
    archive_write_disk_set_standard_lookup(writer);

    if (archive_read_open(reader, &src, nullptr, payloadReadCallback, nullptr) != ARCHIVE_OK) {
        const auto msg = std::string(archive_error_string(reader));
        archive_read_free(reader);
        archive_write_free(writer);
        throw std::runtime_error("Failed to open bundle payload: " + msg);
    }

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
            throw std::runtime_error("Bundle entry escapes extraction directory: " +
                                     entryName.string());
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
                const auto msg = std::string(archive_error_string(writer));
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

}  // namespace aegis
