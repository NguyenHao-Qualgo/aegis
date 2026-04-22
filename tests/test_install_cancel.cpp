#include <gtest/gtest.h>

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "fakes.hpp"
#include "aegis/config/state_store.hpp"
#include "aegis/core/ota_context.hpp"
#include "aegis/core/ota_state_machine.hpp"
#include "aegis/installer/cancel.hpp"
#include "aegis/installer/install_context.hpp"
#include "aegis/installer/payload_streamer.hpp"
#include "aegis/states/idle_state.hpp"

using namespace aegis;
using namespace aegis::test;

namespace {

class InstallCancelTest : public ::testing::Test {
protected:
    std::string store_path_ = "/tmp/aegis_unittest_install_cancel.env";
    std::string payload_path_ = "/tmp/aegis_unittest_install_cancel_payload.bin";

    void TearDown() override {
        std::filesystem::remove(store_path_);
        std::filesystem::remove(payload_path_);
    }

    OtaStateMachine make_machine() const {
        OtaConfig cfg;
        cfg.data_directory = "/tmp/aegis_unittest_install_cancel";
        OtaContext ctx(std::move(cfg), std::make_unique<FakeBootControl>(), nullptr);
        return OtaStateMachine(std::move(ctx),
                               StateStore(store_path_),
                               std::make_unique<IdleState>());
    }
};

TEST_F(InstallCancelTest, StopTokenCancelsPlainPayloadMidStream) {
    auto machine = make_machine();
    CancelSource cancel_source;
    InstallContext ctx{machine, cancel_source.token(), nullptr};
    PayloadStreamer streamer(ctx);

    std::vector<char> payload(kIoBufferSize * 2, 'x');
    {
        std::ofstream out(payload_path_, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        ASSERT_TRUE(out.good());
    }

    const int rfd = ::open(payload_path_.c_str(), O_RDONLY);
    ASSERT_GE(rfd, 0);

    StreamReader reader(rfd);
    CpioEntry entry;
    entry.name = "rootfs.ext4";
    entry.size = payload.size();

    std::string written;
    written.reserve(payload.size());

    auto sink = [&](const char* data, std::size_t len) {
        written.append(data, len);
        cancel_source.request();
    };

    EXPECT_THROW(streamer.stream_plain(reader, entry, sink, ""), Error);
    EXPECT_EQ(written.size(), kIoBufferSize);

    ::close(rfd);
}

TEST_F(InstallCancelTest, SignalFlagCancelsInstallContext) {
    auto machine = make_machine();
    volatile sig_atomic_t signal_stop = 1;
    InstallContext ctx{machine, {}, &signal_stop};

    EXPECT_THROW(ctx.check_cancel(), Error);
}

}  // namespace
