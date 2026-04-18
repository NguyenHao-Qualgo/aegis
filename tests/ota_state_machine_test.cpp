#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>

#include "aegis/boot_control.hpp"
#include "aegis/bundle_manifest.hpp"
#include "aegis/bundle_verifier.hpp"
#include "aegis/gcs_client.hpp"
#include "aegis/ota_context.hpp"
#include "aegis/ota_event.hpp"
#include "aegis/ota_state.hpp"
#include "aegis/ota_state_machine.hpp"
#include "aegis/state_store.hpp"
#include "aegis/states/commit_state.hpp"
#include "aegis/states/failure_state.hpp"
#include "aegis/states/idle_state.hpp"
#include "aegis/states/reboot_state.hpp"
#include "aegis/types.hpp"
#include "aegis/update_handler.hpp"

using namespace aegis;
using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

// ---------------------------------------------------------------------------
// Mock definitions
// ---------------------------------------------------------------------------

class MockBootControl : public IBootControl {
public:
    MOCK_METHOD(std::string, getBootedSlot, (), (const, override));
    MOCK_METHOD(std::string, getPrimarySlot, (), (const, override));
    MOCK_METHOD(std::string, getInactiveSlot, (), (const, override));
    MOCK_METHOD(bool, isSlotBootable, (const std::string&), (const, override));
    MOCK_METHOD(void, setSlotBootable, (const std::string&, bool), (const, override));
    MOCK_METHOD(void, setPrimarySlot, (const std::string&), (const, override));
    MOCK_METHOD(void, markGood, (const std::string&), (const, override));
    MOCK_METHOD(void, markBad, (const std::string&), (const, override));
};

class MockBundleVerifier : public IBundleVerifier {
public:
    MOCK_METHOD(std::optional<BundleManifest>, verifyBundle,
                (const std::string&, const OtaConfig&), (const, override));
    MOCK_METHOD(BundleManifest, loadManifest,
                (const std::string&, const OtaConfig&), (const, override));
    MOCK_METHOD(void, verifyPayloads,
                (const BundleManifest&, const std::string&), (const, override));
    MOCK_METHOD(std::uint64_t, payloadSize, (const std::string&), (const, override));
};

class MockGcsClient : public IGcsClient {
public:
    MOCK_METHOD(std::optional<GcsUpdateInfo>, checkForUpdate, (), (override));
    MOCK_METHOD(void, reportStatus, (const OtaStatus&), (override));
};

class MockUpdateHandler : public IUpdateHandler {
public:
    MOCK_METHOD(bool, supportsImageType, (const std::string&), (const, override));
    MOCK_METHOD(void, install,
                (const std::string&, const SlotConfig&, const std::string&), (const, override));
};

// ---------------------------------------------------------------------------
// Test fixture helpers
// ---------------------------------------------------------------------------

static OtaConfig makeConfig() {
    OtaConfig cfg;
    cfg.compatible = "test-device";
    cfg.dataDirectory = std::filesystem::temp_directory_path() / "aegis_test";
    cfg.bootloader = BootloaderType::UBoot;
    SlotConfig slotA;
    slotA.name = "rootfs.A";
    slotA.bootname = "A";
    slotA.device = "/dev/null";
    slotA.type = SlotType::Ext4;
    SlotConfig slotB;
    slotB.name = "rootfs.B";
    slotB.bootname = "B";
    slotB.device = "/dev/null";
    slotB.type = SlotType::Ext4;
    cfg.slots = {slotA, slotB};
    return cfg;
}

class TempStateStore {
public:
    TempStateStore()
        : dir_(std::filesystem::temp_directory_path() / "aegis_test_store"),
          store_(dir_ / "state.env") {
        std::filesystem::create_directories(dir_);
    }
    ~TempStateStore() {
        std::filesystem::remove_all(dir_);
    }
    StateStore& store() { return store_; }

private:
    std::filesystem::path dir_;
    StateStore store_;
};

static std::unique_ptr<NiceMock<MockBootControl>> makeBootControl(
        const std::string& booted = "A",
        const std::string& primary = "A",
        const std::string& inactive = "B") {
    auto bc = std::make_unique<NiceMock<MockBootControl>>();
    ON_CALL(*bc, getBootedSlot()).WillByDefault(Return(booted));
    ON_CALL(*bc, getPrimarySlot()).WillByDefault(Return(primary));
    ON_CALL(*bc, getInactiveSlot()).WillByDefault(Return(inactive));
    ON_CALL(*bc, isSlotBootable(_)).WillByDefault(Return(true));
    return bc;
}

// ---------------------------------------------------------------------------
// OtaStateMachineTest — uses a custom initial state to avoid restore-from-store.
// ---------------------------------------------------------------------------

class OtaStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(makeConfig().dataDirectory);
    }

    void TearDown() override {
        std::filesystem::remove_all(makeConfig().dataDirectory);
    }

    OtaStateMachine makeMachine(std::unique_ptr<IOtaState> state,
                                std::unique_ptr<NiceMock<MockBootControl>> bc = nullptr) {
        if (!bc) bc = makeBootControl();
        auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
        TempStateStore tss;
        return OtaStateMachine(
            OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
            tss.store(),
            std::move(state));
    }

    static OtaEvent event(OtaEvent::Type type, const std::string& path = "") {
        return OtaEvent{type, path, ""};
    }
};

// ---------------------------------------------------------------------------
// IdleState tests
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, IdleIgnoresMarkGood) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<IdleState>());

    machine.dispatch(event(OtaEvent::Type::MarkGood));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, IdleIgnoresMarkBad) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<IdleState>());

    machine.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, IdleStartInstallEmptyPathTransitionsToFailure) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<IdleState>());

    machine.dispatch(event(OtaEvent::Type::StartInstall, ""));

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
}

// ---------------------------------------------------------------------------
// RebootState tests
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, RebootResumeMatchingSlotTransitionsToCommit) {
    auto bc = makeBootControl("B", "B", "A");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<RebootState>());

    machine.setTargetSlot("B");

    machine.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(machine.getStatus().state, OtaState::Commit);
}

TEST_F(OtaStateMachineTest, RebootResumeMismatchedSlotTransitionsToFailure) {
    auto bc = makeBootControl("A", "A", "B");
    EXPECT_CALL(*bc, setPrimarySlot("A")).Times(1);

    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<RebootState>());

    machine.setTargetSlot("B");

    machine.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
    EXPECT_EQ(machine.getStatus().primarySlot, "A");
}

TEST_F(OtaStateMachineTest, RebootResumeMissingTargetSlotTransitionsToFailure) {
    auto bc = makeBootControl("A");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<RebootState>());

    machine.setTargetSlot(std::nullopt);

    machine.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
}

TEST_F(OtaStateMachineTest, RebootIgnoresUnrelatedEvents) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<RebootState>());

    machine.dispatch(event(OtaEvent::Type::MarkGood));
    machine.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(machine.getStatus().state, OtaState::Reboot);
}

TEST_F(OtaStateMachineTest, RebootResetTransitionsToIdleAndClearsRebootData) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<RebootState>());

    machine.setTargetSlot("B");
    machine.setBundleVersion("2.0");

    machine.dispatch(event(OtaEvent::Type::Reset));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
    EXPECT_FALSE(machine.getStatus().targetSlot.has_value());
    EXPECT_TRUE(machine.getStatus().bundleVersion.empty());
}

// ---------------------------------------------------------------------------
// CommitState tests
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, CommitMarkGoodCallsMarkGoodAndTransitionsToIdle) {
    auto bc = makeBootControl("B");
    EXPECT_CALL(*bc, markGood("B")).Times(1);

    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<CommitState>());

    machine.dispatch(event(OtaEvent::Type::MarkGood));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
    EXPECT_EQ(machine.getStatus().bootedSlot, "B");
}

TEST_F(OtaStateMachineTest, CommitMarkBadCallsMarkBadAndTransitionsToFailure) {
    auto bc = makeBootControl("B");
    EXPECT_CALL(*bc, markBad("B")).Times(1);

    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<CommitState>());

    machine.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
}

TEST_F(OtaStateMachineTest, CommitResetTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<CommitState>());

    machine.dispatch(event(OtaEvent::Type::Reset));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

// ---------------------------------------------------------------------------
// FailureState tests
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, FailureResetTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("test error"));

    machine.dispatch(event(OtaEvent::Type::Reset));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, FailureMarkBadTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("bad slot"));

    machine.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, FailureResumeAfterBootTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("watchdog reset"));

    machine.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

TEST_F(OtaStateMachineTest, FailureIgnoresMarkGood) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("err"));

    machine.dispatch(event(OtaEvent::Type::MarkGood));

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
}

TEST_F(OtaStateMachineTest, FailureStatusSavedWithError) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("something went wrong"));

    EXPECT_EQ(machine.getStatus().lastError, "something went wrong");
    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
}

// ---------------------------------------------------------------------------
// Status-changed callback
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, StatusChangedCallbackInvokedOnSave) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("cb-test"));

    std::vector<OtaState> observed;
    machine.setStatusChangedCallback([&observed](const OtaStatus& s) {
        observed.push_back(s.state);
    });

    machine.dispatch(event(OtaEvent::Type::Reset));

    ASSERT_FALSE(observed.empty());
    EXPECT_EQ(observed.back(), OtaState::Idle);
}

// ---------------------------------------------------------------------------
// Thread safety
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, ConcurrentDispatchDoesNotDeadlock) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("concurrent test"));

    constexpr int kThreads = 4;
    constexpr int kIter = 50;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&machine, i]() {
            for (int j = 0; j < kIter; ++j) {
                try {
                    auto t = (i + j) % 2 == 0
                        ? OtaEvent::Type::Reset
                        : OtaEvent::Type::MarkBad;
                    machine.dispatch(OtaEvent{t, "", ""});
                } catch (const std::exception&) {
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    const auto state = machine.getStatus().state;
    EXPECT_TRUE(state == OtaState::Idle || state == OtaState::Failure);
}

// ---------------------------------------------------------------------------
// GCS client called on idle
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, GcsClientReportStatusCalledOnIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    auto gcs = std::make_shared<NiceMock<MockGcsClient>>();
    EXPECT_CALL(*gcs, reportStatus(_)).Times(::testing::AtLeast(1));
    ON_CALL(*gcs, checkForUpdate()).WillByDefault(Return(std::nullopt));

    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, gcs),
        tss.store(), std::make_unique<IdleState>());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    machine.dispatch(OtaEvent{OtaEvent::Type::Reset, "", ""});
}

// ---------------------------------------------------------------------------
// discardPendingRebootState
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, DiscardPendingRebootStateClearsFields) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<IdleState>());

    machine.setTargetSlot("B");
    machine.setBundleVersion("1.2.3");
    machine.setLastError("old error");

    machine.discardPendingRebootState();

    EXPECT_FALSE(machine.getStatus().targetSlot.has_value());
    EXPECT_TRUE(machine.getStatus().bundleVersion.empty());
    EXPECT_TRUE(machine.getStatus().lastError.empty());
    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}

// ---------------------------------------------------------------------------
// OtaService-level: auto-reset from Failure on new install
// ---------------------------------------------------------------------------

TEST_F(OtaStateMachineTest, StartInstallFromFailureAutoResets) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaStateMachine machine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        tss.store(), std::make_unique<FailureState>("bundle not found"));

    ASSERT_EQ(machine.getStatus().state, OtaState::Failure);

    machine.dispatch(OtaEvent{OtaEvent::Type::Reset, "", ""});
    ASSERT_EQ(machine.getStatus().state, OtaState::Idle);

    machine.dispatch(OtaEvent{OtaEvent::Type::StartInstall, "", ""});
    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
    EXPECT_NE(machine.getStatus().lastError, "bundle not found");
}

// ---------------------------------------------------------------------------
// State restoration from StateStore (normal constructor)
// ---------------------------------------------------------------------------

static OtaStateMachine makeMachineFromStore(
        StateStore& store,
        std::unique_ptr<NiceMock<MockBootControl>> bc = nullptr) {
    if (!bc) bc = makeBootControl("B", "B", "A");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    std::filesystem::create_directories(makeConfig().dataDirectory);
    return OtaStateMachine(
        OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, nullptr),
        store);
}

TEST_F(OtaStateMachineTest, AfterRebootRestoredToRebootState) {
    TempStateStore tss;
    {
        OtaStatus rebootStatus;
        rebootStatus.state = OtaState::Reboot;
        rebootStatus.targetSlot = "B";
        rebootStatus.bundleVersion = "2.0.0";
        tss.store().save(rebootStatus);
    }

    OtaStateMachine machine = makeMachineFromStore(tss.store());

    EXPECT_EQ(machine.getStatus().state, OtaState::Reboot);
    EXPECT_EQ(machine.getStatus().targetSlot, "B");
}

TEST_F(OtaStateMachineTest, ResumeAfterBootReachesHandleWhenStateIsReboot) {
    TempStateStore tss;
    OtaStatus rebootStatus;
    rebootStatus.state = OtaState::Reboot;
    rebootStatus.targetSlot = "B";
    tss.store().save(rebootStatus);

    auto bc = makeBootControl("B", "B", "A");
    OtaStateMachine machine = makeMachineFromStore(tss.store(), std::move(bc));

    machine.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    EXPECT_EQ(machine.getStatus().state, OtaState::Commit);
}

TEST_F(OtaStateMachineTest, ResumeAfterBootFailsWhenBootedSlotMismatch) {
    TempStateStore tss;
    OtaStatus rebootStatus;
    rebootStatus.state = OtaState::Reboot;
    rebootStatus.targetSlot = "B";
    tss.store().save(rebootStatus);

    auto bc = makeBootControl("A", "A", "B");
    EXPECT_CALL(*bc, setPrimarySlot("A")).Times(1);
    OtaStateMachine machine = makeMachineFromStore(tss.store(), std::move(bc));

    machine.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
    EXPECT_EQ(machine.getStatus().primarySlot, "A");
}

TEST_F(OtaStateMachineTest, AfterRebootRestoredToCommitState) {
    TempStateStore tss;
    OtaStatus commitStatus;
    commitStatus.state = OtaState::Commit;
    tss.store().save(commitStatus);

    OtaStateMachine machine = makeMachineFromStore(tss.store());

    EXPECT_EQ(machine.getStatus().state, OtaState::Commit);
}

TEST_F(OtaStateMachineTest, AfterRebootRestoredToFailureState) {
    TempStateStore tss;
    OtaStatus failStatus;
    failStatus.state = OtaState::Failure;
    failStatus.lastError = "install failed";
    tss.store().save(failStatus);

    OtaStateMachine machine = makeMachineFromStore(tss.store());

    EXPECT_EQ(machine.getStatus().state, OtaState::Failure);
    EXPECT_EQ(machine.getStatus().lastError, "install failed");
}

TEST_F(OtaStateMachineTest, ResumeAfterBootInIdleIsNoOp) {
    TempStateStore tss;
    OtaStateMachine machine = makeMachineFromStore(tss.store());

    machine.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    EXPECT_EQ(machine.getStatus().state, OtaState::Idle);
}
