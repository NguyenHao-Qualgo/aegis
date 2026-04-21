#include "aegis/packer.hpp"
#include "aegis/types.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace aegis {

namespace {

constexpr std::size_t kCpioBlockSize = 512;

std::string base_name(const std::string &path) {
    fs::path p(path);
    std::string name = p.filename().string();
    if (name.empty()) {
        fail_runtime("invalid payload path: " + path);
    }
    return name;
}

void write_padding(std::ostream &output, std::size_t size, std::size_t alignment) {
    const std::size_t pad = (alignment - (size % alignment)) % alignment;
    static const std::array<char, 4> zeros = {0, 0, 0, 0};
    output.write(zeros.data(), static_cast<std::streamsize>(pad));
}

std::string hex_field(std::uint64_t value) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << (value & 0xFFFFFFFFULL);
    return oss.str();
}

std::uint32_t checksum_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail_runtime("cannot open payload for checksum: " + path);
    }

    std::array<char, kIoBufferSize> buffer{};
    std::uint32_t checksum = 0;
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            checksum += static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
        }
    }
    return checksum;
}

std::uint64_t file_size_of(const std::string &path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        fail_runtime("cannot stat file size: " + path);
    }
    return size;
}

std::uint64_t file_mtime_of(const std::string &path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
        fail_runtime("cannot stat file: " + path);
    }
    return static_cast<std::uint64_t>(st.st_mtime);
}

std::uint64_t file_mode_of(const std::string &path) {
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
        fail_runtime("cannot stat file mode: " + path);
    }
    return static_cast<std::uint64_t>(st.st_mode);
}

void copy_file_contents(std::ostream &output, const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail_runtime("cannot open input file: " + path);
    }

    std::array<char, kIoBufferSize> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            output.write(buffer.data(), count);
        }
    }

    if (!output) {
        fail_runtime("failed while writing archive");
    }
}

void write_cpio_entry(std::ostream &output,
                      const std::string &archive_name,
                      const std::string *source_path,
                      std::uint32_t inode) {
    const std::uint64_t size   = source_path ? file_size_of(*source_path) : 0;
    const std::uint64_t mtime  = source_path ? file_mtime_of(*source_path) : 0;
    const std::uint64_t mode   = source_path ? file_mode_of(*source_path) : 0100644;
    const std::uint32_t cksum  = source_path ? checksum_file(*source_path) : 0;
    const std::uint64_t namesz = archive_name.size() + 1;

    output << "070702"
           << hex_field(inode)
           << hex_field(mode)
           << hex_field(0)
           << hex_field(0)
           << hex_field(1)
           << hex_field(mtime)
           << hex_field(size)
           << hex_field(0)
           << hex_field(0)
           << hex_field(0)
           << hex_field(0)
           << hex_field(namesz)
           << hex_field(cksum);

    output.write(archive_name.c_str(), static_cast<std::streamsize>(archive_name.size()));
    output.put('\0');
    write_padding(output, kCpioHeaderSize + namesz, 4);

    if (source_path) {
        copy_file_contents(output, *source_path);
        write_padding(output, size, 4);
    }
}

}  // namespace

Packer::Packer(const PackOptions &options) : options_(options) {}

int Packer::pack() {
    std::ofstream output(options_.output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        fail_runtime("cannot create output archive: " + options_.output_path);
    }

    std::uint32_t inode = 1;

    write_cpio_entry(output, "sw-description", &options_.sw_description, inode++);
    if (!options_.sw_description_sig.empty()) {
        write_cpio_entry(output, "sw-description.sig", &options_.sw_description_sig, inode++);
    }

    for (const auto &payload : options_.payloads) {
        const auto archive_name = base_name(payload);
        if (archive_name == "sw-description" || archive_name == "sw-description.sig") {
            fail_runtime("payload name is reserved inside the archive: " + archive_name);
        }
        write_cpio_entry(output, archive_name, &payload, inode++);
    }

    write_cpio_entry(output, kTrailerName, nullptr, inode);

    const std::streamoff current_size = output.tellp();
    if (current_size < 0) {
        fail_runtime("failed to finalize output archive");
    }
    write_padding(output, static_cast<std::size_t>(current_size), kCpioBlockSize);
    output.flush();

    if (!output) {
        fail_runtime("failed to write output archive");
    }

    return EXIT_SUCCESS;
}

}  // namespace aegis
