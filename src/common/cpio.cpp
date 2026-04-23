#include "aegis/common/cpio.hpp"

#include <cstring>

namespace aegis {

namespace {

std::uint32_t from_hex_field(const char *field, std::size_t len) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < len; ++i) {
        value <<= 4U;
        const unsigned char c = static_cast<unsigned char>(field[i]);
        if      (c >= '0' && c <= '9') { value |= static_cast<std::uint32_t>(c - '0'); }
        else if (c >= 'A' && c <= 'F') { value |= static_cast<std::uint32_t>(10 + c - 'A'); }
        else if (c >= 'a' && c <= 'f') { value |= static_cast<std::uint32_t>(10 + c - 'a'); }
        else                           { fail_runtime("invalid cpio hex field"); }
    }
    return value;
}

}  // namespace

void skip_padding(StreamReader &reader, std::uint64_t size, std::size_t alignment) {
    const std::uint64_t pad = (alignment - (size % alignment)) % alignment;
    if (pad != 0) { reader.skip(pad); }
}

CpioEntry read_cpio_entry(StreamReader &reader) {
    std::array<char, kCpioHeaderSize> header{};
    reader.read_exact(header.data(), header.size());
    if (std::strncmp(header.data(), "070701", 6) != 0 && std::strncmp(header.data(), "070702", 6) != 0) {
        fail_runtime("unsupported cpio format");
    }
    const auto namesize = from_hex_field(header.data() + 94, 8);
    const auto filesize = from_hex_field(header.data() + 54, 8);
    const auto checksum = from_hex_field(header.data() + 102, 8);
    if (namesize == 0) { fail_runtime("invalid cpio filename"); }

    std::string name = reader.read_string(namesize);
    if (!name.empty() && name.back() == '\0') { name.pop_back(); }
    skip_padding(reader, kCpioHeaderSize + namesize);
    return CpioEntry{std::move(name), filesize, checksum};
}

}  // namespace aegis
