#include <gtest/gtest.h>
#include "fakes.hpp"
#include "aegis/core/ota_state_machine.hpp"
#include "aegis/core/ota_service.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/states/commit_state.hpp"
#include "aegis/states/download_state.hpp"
#include <filesystem>

using namespace aegis;
using namespace aegis::test;

class OtaStateMachineTest : public ::testing::Test {
protected:
    const std::string store_path{"/tmp/aegis_unittest_sm_12345.env"};

    void TearDown() override { std::filesystem::remove(store_path); }

    // Creates a machine with a given initial state (never uses persisted store).
    // gcsClient = nullptr so IdleState won't spawn a polling thread.
    std::unique_ptr<OtaStateMachine> make(std::unique_ptr<IOtaState> init) {
        OtaConfig cfg;
        cfg.data_directory = "/tmp/aegis_unittest_sm_data";
        auto boot = std::make_unique<FakeBootControl>();
        OtaContext ctx(std::move(cfg), std::move(boot), nullptr);
        StateStore store(store_path);
        return std::make_unique<OtaStateMachine>(std::move(ctx), std::move(store), std::move(init));
    }
};

TEST_F(OtaStateMachineTest, StartsIdle) {
    auto m = make(std::make_unique<IdleState>());
    EXPECT_EQ(m->getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, BootedSlotPopulated) {
    auto m = make(std::make_unique<IdleState>());
    EXPECT_EQ(m->getStatus().bootedSlot, "A");
}

TEST_F(OtaStateMachineTest, SetProgress) {
    auto m = make(std::make_unique<IdleState>());
    m->setProgress(OtaState::Download, "downloading", 33, "fetching...");
    const auto s = m->getStatus();
    EXPECT_EQ(s.state,     OtaState::Download);
    EXPECT_EQ(s.operation, "downloading");
    EXPECT_EQ(s.progress,  33);
    EXPECT_EQ(s.message,   "fetching...");
}

TEST_F(OtaStateMachineTest, SetIdleResetsProgress) {
    auto m = make(std::make_unique<IdleState>());
    m->setProgress(OtaState::Download, "dl", 50, "");
    m->setIdle("done");
    const auto s = m->getStatus();
    EXPECT_EQ(s.state,    OtaState::Idle);
    EXPECT_EQ(s.progress, 0);
    EXPECT_EQ(s.message,  "done");
}

TEST_F(OtaStateMachineTest, SetFailure) {
    auto m = make(std::make_unique<IdleState>());
    m->setFailure("disk full", "OTA failed");
    const auto s = m->getStatus();
    EXPECT_EQ(s.state,     OtaState::Failure);
    EXPECT_EQ(s.lastError, "disk full");
    EXPECT_EQ(s.message,   "OTA failed");
}

TEST_F(OtaStateMachineTest, SetBundlePath) {
    auto m = make(std::make_unique<IdleState>());
    m->setBundlePath("/tmp/bundle.swu");
    EXPECT_EQ(m->getStatus().bundlePath, "/tmp/bundle.swu");
}

TEST_F(OtaStateMachineTest, SetInstallAndClear) {
    auto m = make(std::make_unique<IdleState>());
    m->setInstallPath("/tmp/install");
    EXPECT_EQ(m->getStatus().installPath, "/tmp/install");
    m->clearInstallPath();
    EXPECT_TRUE(m->getStatus().installPath.empty());
}

TEST_F(OtaStateMachineTest, SetTargetSlot) {
    auto m = make(std::make_unique<IdleState>());
    m->setTargetSlot("B");
    EXPECT_EQ(m->getStatus().targetSlot, "B");
    m->setTargetSlot(std::nullopt);
    EXPECT_FALSE(m->getStatus().targetSlot.has_value());
}

TEST_F(OtaStateMachineTest, SetBundleVersion) {
    auto m = make(std::make_unique<IdleState>());
    m->setBundleVersion("2.0.1");
    EXPECT_EQ(m->getStatus().bundleVersion, "2.0.1");
}

TEST_F(OtaStateMachineTest, SetAndClearLastError) {
    auto m = make(std::make_unique<IdleState>());
    m->setLastError("network timeout");
    EXPECT_EQ(m->getStatus().lastError, "network timeout");
    m->clearLastError();
    EXPECT_TRUE(m->getStatus().lastError.empty());
}

TEST_F(OtaStateMachineTest, UpdateSlots) {
    auto m = make(std::make_unique<IdleState>());
    m->updateSlots("B", "B");
    const auto s = m->getStatus();
    EXPECT_EQ(s.bootedSlot,  "B");
    EXPECT_EQ(s.primarySlot, "B");
}

TEST_F(OtaStateMachineTest, ClearWorkflowData) {
    auto m = make(std::make_unique<IdleState>());
    m->setBundleVersion("3.0");
    m->setLastError("err");
    m->setTargetSlot("B");
    m->clearWorkflowData();
    const auto s = m->getStatus();
    EXPECT_FALSE(s.targetSlot.has_value());
    EXPECT_TRUE(s.bundleVersion.empty());
    EXPECT_TRUE(s.lastError.empty());
}

TEST_F(OtaStateMachineTest, StatusChangedCallback) {
    auto m = make(std::make_unique<IdleState>());
    OtaStatus captured;
    m->setStatusChangedCallback([&](const OtaStatus& s) { captured = s; });

    m->setProgress(OtaState::Install, "installing", 75, "writing...");
    EXPECT_EQ(captured.state,    OtaState::Install);
    EXPECT_EQ(captured.progress, 75);
}

TEST_F(OtaStateMachineTest, MarkActiveUpdatesPrimary) {
    auto m = make(std::make_unique<IdleState>());
    EXPECT_NO_THROW(m->markActive("B"));
    EXPECT_EQ(m->getStatus().primarySlot, "B");
}

TEST_F(OtaStateMachineTest, MarkActiveUnbootableThrows) {
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    auto boot = std::make_unique<FakeBootControl>();
    boot->bootable = false;
    OtaContext ctx(std::move(cfg), std::move(boot), nullptr);
    StateStore store(store_path);
    OtaStateMachine m(std::move(ctx), std::move(store), std::make_unique<IdleState>());

    EXPECT_THROW(m.markActive("B"), std::runtime_error);
}

TEST_F(OtaStateMachineTest, PersistenceReboot) {
    {
        auto m = make(std::make_unique<IdleState>());
        m->setTargetSlot("B");
        m->setProgress(OtaState::Reboot, "rebooting", 95, "waiting for reboot");
    }
    // New machine picks up persisted Reboot state
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    auto boot = std::make_unique<FakeBootControl>();
    OtaContext ctx(std::move(cfg), std::move(boot), nullptr);
    StateStore store(store_path);
    OtaStateMachine m(std::move(ctx), std::move(store));
    EXPECT_EQ(m.getStatus().state, OtaState::Reboot);
    ASSERT_TRUE(m.getStatus().targetSlot.has_value());
    EXPECT_EQ(*m.getStatus().targetSlot, "B");
}

TEST_F(OtaStateMachineTest, ResumeAfterBootFromPersistedRebootUsesPersistedTargetSlot) {
    {
        auto m = make(std::make_unique<IdleState>());
        m->setTargetSlot("B");
        m->setProgress(OtaState::Reboot, "rebooting", 100, "waiting for reboot");
    }

    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    auto boot = std::make_unique<FakeBootControl>();
    boot->booted = "B";
    boot->primary = "B";
    OtaContext ctx(std::move(cfg), std::move(boot), nullptr);
    StateStore store(store_path);
    OtaStateMachine m(std::move(ctx), std::move(store));

    m.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    const auto status = m.getStatus();
    EXPECT_EQ(status.state, OtaState::Idle);
    EXPECT_EQ(status.bootedSlot, "B");
    EXPECT_EQ(status.primarySlot, "B");
    EXPECT_TRUE(status.lastError.empty());
}

TEST_F(OtaStateMachineTest, PersistenceFailure) {
    // FailureState::onEnter immediately transitions to IdleState after reporting,
    // so a machine restored from a persisted Failure ends up in Idle.
    {
        auto m = make(std::make_unique<IdleState>());
        m->setFailure("install error", "OTA failed");
    }
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    OtaContext ctx(std::move(cfg), std::make_unique<FakeBootControl>(), nullptr);
    OtaStateMachine m(std::move(ctx), StateStore(store_path));
    EXPECT_EQ(m.getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, ConfigAccessible) {
    auto m = make(std::make_unique<IdleState>());
    EXPECT_EQ(m->config().data_directory, "/tmp/aegis_unittest_sm_data");
}

TEST_F(OtaStateMachineTest, GcsClientNullWhenNotProvided) {
    auto m = make(std::make_unique<IdleState>());
    EXPECT_EQ(m->gcsClient(), nullptr);
}

class OtaServiceTest : public ::testing::Test {
protected:
    const std::string store_path{"/tmp/aegis_unittest_svc_12345.env"};

    void TearDown() override { std::filesystem::remove(store_path); }
};

TEST_F(OtaServiceTest, GetStatusIdle) {
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    OtaService svc(std::move(cfg),
                   std::make_unique<FakeBootControl>(),
                   StateStore(store_path),
                   nullptr);
    EXPECT_EQ(svc.getStatus().state, OtaState::Idle);
}

TEST_F(OtaServiceTest, GetBootedSlot) {
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    OtaService svc(std::move(cfg),
                   std::make_unique<FakeBootControl>(),
                   StateStore(store_path),
                   nullptr);
    EXPECT_EQ(svc.getBooted(), "A");
}

TEST_F(OtaServiceTest, GetPrimarySlot) {
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    OtaService svc(std::move(cfg),
                   std::make_unique<FakeBootControl>(),
                   StateStore(store_path),
                   nullptr);
    EXPECT_EQ(svc.getPrimary(), "A");
}

TEST_F(OtaServiceTest, StatusCallback) {
    OtaConfig cfg;
    cfg.data_directory = "/tmp";
    OtaService svc(std::move(cfg),
                   std::make_unique<FakeBootControl>(),
                   StateStore(store_path),
                   nullptr);
    bool saw_non_idle = false;
    svc.setStatusChangedCallback([&](const OtaStatus& s) {
        if (s.state != OtaState::Idle) {
            saw_non_idle = true;
        }
    });
    // startInstall dispatches async work. The final callback may already be Idle
    // again if the install fails quickly, so assert that we observed any non-idle
    // status during the callback stream.
    svc.startInstall("/tmp/bundle.swu");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(saw_non_idle);
}
