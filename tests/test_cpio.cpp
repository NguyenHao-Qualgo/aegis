#include <gtest/gtest.h>
#include "aegis/common/cpio.hpp"
#include "aegis/common/error.hpp"
#include <array>
#include <cstring>
#include <unistd.h>

using namespace aegis;

static std::string hex8(uint64_t v) {
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08X", static_cast<uint32_t>(v));
    return {buf, 8};
}

// Builds a 110-byte CPIO newc header string.
static std::string make_cpio_header(const std::string& name,
                                    uint64_t filesize,
                                    uint32_t checksum = 0) {
    const uint64_t namesz = name.size() + 1;  // include null terminator
    std::string h;
    h += "070701";         // magic
    h += hex8(1);          // ino
    h += hex8(0x81A4);     // mode (regular file 0644)
    h += hex8(0);          // uid
    h += hex8(0);          // gid
    h += hex8(1);          // nlink
    h += hex8(0);          // mtime
    h += hex8(filesize);   // filesize  — offset 54
    h += hex8(0);          // devmajor
    h += hex8(0);          // devminor
    h += hex8(0);          // rdevmajor
    h += hex8(0);          // rdevminor
    h += hex8(namesz);     // namesize  — offset 94
    h += hex8(checksum);   // check     — offset 102
    return h;              // 110 bytes total
}

// Writes CPIO data into a pipe and returns a StreamReader over the read end.
// Caller must close the fd returned by reader.input_fd() when done.
// Returns {read_fd, write_fd} — the caller writes then closes write_fd.
static std::pair<int,int> open_pipe() {
    int fds[2];
    if (::pipe(fds) != 0) throw std::runtime_error("pipe failed");
    return {fds[0], fds[1]};
}

TEST(SkipPaddingTest, AlreadyAligned) {
    // size=8, alignment=4 → pad = (4 - 8%4)%4 = 0 bytes
    auto [rfd, wfd] = open_pipe();
    ::close(wfd);  // no padding bytes needed
    StreamReader reader(rfd);
    EXPECT_NO_THROW(skip_padding(reader, 8, 4));
    ::close(rfd);
}

TEST(SkipPaddingTest, ThreeBytePadding) {
    // size=9, alignment=4 → pad = 3
    auto [rfd, wfd] = open_pipe();
    const char pad[3] = {0, 0, 0};
    ::write(wfd, pad, 3);
    ::close(wfd);
    StreamReader reader(rfd);
    EXPECT_NO_THROW(skip_padding(reader, 9, 4));
    ::close(rfd);
}

TEST(SkipPaddingTest, OneBytePadding) {
    // size=7, alignment=4 → pad = 1
    auto [rfd, wfd] = open_pipe();
    const char pad = 0;
    ::write(wfd, &pad, 1);
    ::close(wfd);
    StreamReader reader(rfd);
    EXPECT_NO_THROW(skip_padding(reader, 7, 4));
    ::close(rfd);
}

TEST(ReadCpioEntryTest, ValidEntry) {
    const std::string name     = "hello.txt";
    const uint64_t    filesize = 13;

    const std::string header  = make_cpio_header(name, filesize);
    ASSERT_EQ(header.size(), 110u);

    // name + null, then padding so that (110 + namesz) is 4-byte aligned
    std::string namedata = name + '\0';                   // 10 bytes
    const size_t total_so_far = 110 + namedata.size();    // 120 → already multiple of 4
    const size_t pad = (4 - (total_so_far % 4)) % 4;

    auto [rfd, wfd] = open_pipe();
    ::write(wfd, header.c_str(), header.size());
    ::write(wfd, namedata.c_str(), namedata.size());
    if (pad) { std::array<char,4> z{}; ::write(wfd, z.data(), pad); }
    ::close(wfd);

    StreamReader reader(rfd);
    const auto entry = read_cpio_entry(reader);
    EXPECT_EQ(entry.name, "hello.txt");
    EXPECT_EQ(entry.size, 13u);
    EXPECT_EQ(entry.checksum, 0u);
    ::close(rfd);
}

TEST(ReadCpioEntryTest, TrailerEntry) {
    const std::string name = kTrailerName;  // "TRAILER!!!"
    const std::string header = make_cpio_header(name, 0);
    std::string namedata = name + '\0';     // 11 bytes
    const size_t total = 110 + namedata.size();
    const size_t pad = (4 - (total % 4)) % 4;

    auto [rfd, wfd] = open_pipe();
    ::write(wfd, header.c_str(), header.size());
    ::write(wfd, namedata.c_str(), namedata.size());
    if (pad) { std::array<char,4> z{}; ::write(wfd, z.data(), pad); }
    ::close(wfd);

    StreamReader reader(rfd);
    const auto entry = read_cpio_entry(reader);
    EXPECT_EQ(entry.name, kTrailerName);
    EXPECT_EQ(entry.size, 0u);
    ::close(rfd);
}

TEST(ReadCpioEntryTest, Magic070702Accepted) {
    const std::string name = "x";
    std::string header = make_cpio_header(name, 0);
    header[5] = '2';  // change "070701" to "070702"

    std::string namedata = name + '\0';
    const size_t pad = (4 - ((110 + namedata.size()) % 4)) % 4;

    auto [rfd, wfd] = open_pipe();
    ::write(wfd, header.c_str(), header.size());
    ::write(wfd, namedata.c_str(), namedata.size());
    if (pad) { std::array<char,4> z{}; ::write(wfd, z.data(), pad); }
    ::close(wfd);

    StreamReader reader(rfd);
    EXPECT_NO_THROW(read_cpio_entry(reader));
    ::close(rfd);
}

TEST(ReadCpioEntryTest, InvalidMagicThrows) {
    std::array<char, 110> header{};
    std::memcpy(header.data(), "BADMAG", 6);

    auto [rfd, wfd] = open_pipe();
    ::write(wfd, header.data(), header.size());
    ::close(wfd);

    StreamReader reader(rfd);
    EXPECT_THROW(read_cpio_entry(reader), Error);
    ::close(rfd);
}

TEST(ReadCpioEntryTest, WithNonZeroChecksum) {
    const std::string name = "data.bin";
    const uint32_t cksum = 0xDEADBEEF;
    const std::string header = make_cpio_header(name, 4, cksum);
    std::string namedata = name + '\0';
    const size_t pad = (4 - ((110 + namedata.size()) % 4)) % 4;

    auto [rfd, wfd] = open_pipe();
    ::write(wfd, header.c_str(), header.size());
    ::write(wfd, namedata.c_str(), namedata.size());
    if (pad) { std::array<char,4> z{}; ::write(wfd, z.data(), pad); }
    ::close(wfd);

    StreamReader reader(rfd);
    const auto entry = read_cpio_entry(reader);
    EXPECT_EQ(entry.checksum, cksum);
    ::close(rfd);
}
