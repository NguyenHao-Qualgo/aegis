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

// Builds a minimal OtaConfig with no real filesystem paths.
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

// Builds a StateStore backed by a temp file that is cleaned up automatically.
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

// Convenience: create a NiceMock<MockBootControl> pre-programmed with
// typical "slot A booted, slot A primary, slot B inactive" defaults.
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
// OtaContextTest — uses the test constructor to inject a custom initial state.
// ---------------------------------------------------------------------------

class OtaContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(makeConfig().dataDirectory);
    }

    void TearDown() override {
        std::filesystem::remove_all(makeConfig().dataDirectory);
    }

    // Build a context that starts in `state` with NiceMock boot control.
    OtaContext makeContext(std::unique_ptr<IOtaState> state,
                          std::unique_ptr<NiceMock<MockBootControl>> bc = nullptr) {
        if (!bc) bc = makeBootControl();
        auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
        TempStateStore tss;
        return OtaContext(
            makeConfig(),
            std::move(bc),
            std::move(verifier),
            {},
            tss.store(),
            nullptr,
            std::move(state));
    }

    static OtaEvent event(OtaEvent::Type type, const std::string& path = "") {
        return OtaEvent{type, path, ""};
    }
};

// ---------------------------------------------------------------------------
// IdleState tests
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, IdleIgnoresMarkGood) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<IdleState>());

    ctx.dispatch(event(OtaEvent::Type::MarkGood));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}

TEST_F(OtaContextTest, IdleIgnoresMarkBad) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<IdleState>());

    ctx.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}

TEST_F(OtaContextTest, IdleStartInstallEmptyPathTransitionsToFailure) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<IdleState>());

    ctx.dispatch(event(OtaEvent::Type::StartInstall, ""));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

// ---------------------------------------------------------------------------
// RebootState tests
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, RebootResumeMatchingSlotTransitionsToCommit) {
    auto bc = makeBootControl("B", "B", "A");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;

    auto initialState = std::make_unique<RebootState>();
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::move(initialState));

    ctx.status_.targetSlot = "B";

    ctx.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Commit);
}

TEST_F(OtaContextTest, RebootResumeMismatchedSlotTransitionsToFailure) {
    auto bc = makeBootControl("A", "A", "B");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;

    auto initialState = std::make_unique<RebootState>();
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::move(initialState));

    // We booted into A but expected B.
    ctx.status_.targetSlot = "B";

    ctx.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

TEST_F(OtaContextTest, RebootResumeMissingTargetSlotTransitionsToFailure) {
    auto bc = makeBootControl("A");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;

    auto initialState = std::make_unique<RebootState>();
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::move(initialState));

    // targetSlot not set — this is an invalid state
    ctx.status_.targetSlot = std::nullopt;

    ctx.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

TEST_F(OtaContextTest, RebootIgnoresUnrelatedEvents) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;

    auto initialState = std::make_unique<RebootState>();
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::move(initialState));

    ctx.dispatch(event(OtaEvent::Type::MarkGood));
    ctx.dispatch(event(OtaEvent::Type::MarkBad));
    ctx.dispatch(event(OtaEvent::Type::StartInstall, "/some/path"));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Reboot);
}

// ---------------------------------------------------------------------------
// CommitState tests
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, CommitMarkGoodCallsMarkGoodAndTransitionsToIdle) {
    auto bc = makeBootControl("B");
    EXPECT_CALL(*bc, markGood("B")).Times(1);

    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<CommitState>());

    ctx.dispatch(event(OtaEvent::Type::MarkGood));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
    EXPECT_EQ(ctx.getStatus().bootedSlot, "B");
}

TEST_F(OtaContextTest, CommitMarkBadCallsMarkBadAndTransitionsToFailure) {
    auto bc = makeBootControl("B");
    EXPECT_CALL(*bc, markBad("B")).Times(1);

    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<CommitState>());

    ctx.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

TEST_F(OtaContextTest, CommitResetTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<CommitState>());

    ctx.dispatch(event(OtaEvent::Type::Reset));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}

// ---------------------------------------------------------------------------
// FailureState tests
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, FailureResetTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("test error"));

    ctx.dispatch(event(OtaEvent::Type::Reset));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}

TEST_F(OtaContextTest, FailureMarkBadTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("bad slot"));

    ctx.dispatch(event(OtaEvent::Type::MarkBad));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}

TEST_F(OtaContextTest, FailureResumeAfterBootTransitionsToIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("watchdog reset"));

    ctx.dispatch(event(OtaEvent::Type::ResumeAfterBoot));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}

TEST_F(OtaContextTest, FailureIgnoresMarkGood) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("err"));

    ctx.dispatch(event(OtaEvent::Type::MarkGood));

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

TEST_F(OtaContextTest, FailureStatusSavedWithError) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("something went wrong"));

    EXPECT_EQ(ctx.getStatus().lastError, "something went wrong");
    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

// ---------------------------------------------------------------------------
// Status-changed callback
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, StatusChangedCallbackInvokedOnSave) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("cb-test"));

    std::vector<OtaState> observed;
    ctx.setStatusChangedCallback([&observed](const OtaStatus& s) {
        observed.push_back(s.state);
    });

    // Reset → transitions to Idle → IdleState::onEnter calls ctx.save()
    ctx.dispatch(event(OtaEvent::Type::Reset));

    ASSERT_FALSE(observed.empty());
    EXPECT_EQ(observed.back(), OtaState::Idle);
}

// ---------------------------------------------------------------------------
// Thread safety — concurrent dispatches must not corrupt state
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, ConcurrentDispatchDoesNotDeadlock) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;

    // Alternate between CommitState and FailureState via rapid concurrent events.
    // The test passes if it completes without deadlock or crash.
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<FailureState>("concurrent test"));

    constexpr int kThreads = 4;
    constexpr int kIter = 50;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&ctx, i]() {
            for (int j = 0; j < kIter; ++j) {
                try {
                    // Alternate between Reset (goes to Idle) and MarkBad (in Failure → Idle).
                    auto t = (i + j) % 2 == 0
                        ? OtaEvent::Type::Reset
                        : OtaEvent::Type::MarkBad;
                    ctx.dispatch(OtaEvent{t, "", ""});
                } catch (const std::exception&) {
                    // Concurrent dispatch throws when another is in progress — that's OK.
                }
            }
        });
    }

    for (auto& t : threads) t.join();
    // Reachable final state is either Idle or Failure depending on timing; just verify not crashed.
    const auto state = ctx.getStatus().state;
    EXPECT_TRUE(state == OtaState::Idle || state == OtaState::Failure);
}

// ---------------------------------------------------------------------------
// GCS callback triggers a poll dispatch (no real HTTP call)
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, GcsClientReportStatusCalledOnIdle) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    auto gcs = std::make_shared<NiceMock<MockGcsClient>>();
    EXPECT_CALL(*gcs, reportStatus(_)).Times(::testing::AtLeast(1));
    ON_CALL(*gcs, checkForUpdate()).WillByDefault(Return(std::nullopt));

    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), gcs,
                   std::make_unique<IdleState>());

    // Small sleep to let the onEnter report fire, then transition away to stop poll thread.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ctx.dispatch(OtaEvent{OtaEvent::Type::Reset, "", ""});
}

// ---------------------------------------------------------------------------
// discardPendingRebootState resets relevant fields
// ---------------------------------------------------------------------------

TEST_F(OtaContextTest, DiscardPendingRebootStateClearsFields) {
    auto bc = makeBootControl();
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    TempStateStore tss;
    OtaContext ctx(makeConfig(), std::move(bc), std::move(verifier), {}, tss.store(), nullptr,
                   std::make_unique<IdleState>());

    ctx.status_.targetSlot = "B";
    ctx.status_.bundleVersion = "1.2.3";
    ctx.status_.lastError = "old error";

    ctx.discardPendingRebootState();

    EXPECT_FALSE(ctx.status_.targetSlot.has_value());
    EXPECT_TRUE(ctx.status_.bundleVersion.empty());
    EXPECT_TRUE(ctx.status_.lastError.empty());
    EXPECT_EQ(ctx.status_.state, OtaState::Idle);
}

// ---------------------------------------------------------------------------
// Reboot-resume path (normal constructor restores state from StateStore)
// ---------------------------------------------------------------------------

// Helper: build an OtaContext using the normal constructor so that
// stateFromPersisted() picks up whatever was saved to the store.
static OtaContext makeContextFromStore(StateStore& store,
                                       std::unique_ptr<NiceMock<MockBootControl>> bc = nullptr) {
    if (!bc) bc = makeBootControl("B", "B", "A");
    auto verifier = std::make_unique<NiceMock<MockBundleVerifier>>();
    std::filesystem::create_directories(makeConfig().dataDirectory);
    return OtaContext(makeConfig(), std::move(bc), std::move(verifier), {}, store, nullptr);
}

TEST_F(OtaContextTest, AfterRebootRestoredToRebootState) {
    TempStateStore tss;

    // Simulate: install completed, daemon wrote Reboot state, then system rebooted.
    {
        OtaStatus rebootStatus;
        rebootStatus.state = OtaState::Reboot;
        rebootStatus.targetSlot = "B";
        rebootStatus.bundleVersion = "2.0.0";
        tss.store().save(rebootStatus);
    }

    OtaContext ctx = makeContextFromStore(tss.store());

    EXPECT_EQ(ctx.getStatus().state, OtaState::Reboot);
    EXPECT_EQ(ctx.getStatus().targetSlot, "B");
}

TEST_F(OtaContextTest, ResumeAfterBootReachesHandleWhenStateIsReboot) {
    TempStateStore tss;

    OtaStatus rebootStatus;
    rebootStatus.state = OtaState::Reboot;
    rebootStatus.targetSlot = "B";
    tss.store().save(rebootStatus);

    // On cold boot: booted slot B matches targetSlot B.
    auto bc = makeBootControl("B", "B", "A");
    OtaContext ctx = makeContextFromStore(tss.store(), std::move(bc));

    ctx.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    EXPECT_EQ(ctx.getStatus().state, OtaState::Commit);
}

TEST_F(OtaContextTest, ResumeAfterBootFailsWhenBootedSlotMismatch) {
    TempStateStore tss;

    OtaStatus rebootStatus;
    rebootStatus.state = OtaState::Reboot;
    rebootStatus.targetSlot = "B";
    tss.store().save(rebootStatus);

    // On cold boot: ended up on slot A instead of expected B (watchdog rollback).
    auto bc = makeBootControl("A", "A", "B");
    OtaContext ctx = makeContextFromStore(tss.store(), std::move(bc));

    ctx.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
}

TEST_F(OtaContextTest, AfterRebootRestoredToCommitState) {
    TempStateStore tss;

    OtaStatus commitStatus;
    commitStatus.state = OtaState::Commit;
    tss.store().save(commitStatus);

    OtaContext ctx = makeContextFromStore(tss.store());

    EXPECT_EQ(ctx.getStatus().state, OtaState::Commit);
}

TEST_F(OtaContextTest, AfterRebootRestoredToFailureState) {
    TempStateStore tss;

    OtaStatus failStatus;
    failStatus.state = OtaState::Failure;
    failStatus.lastError = "install failed";
    tss.store().save(failStatus);

    OtaContext ctx = makeContextFromStore(tss.store());

    EXPECT_EQ(ctx.getStatus().state, OtaState::Failure);
    EXPECT_EQ(ctx.getStatus().lastError, "install failed");
}

TEST_F(OtaContextTest, ResumeAfterBootInIdleIsNoOp) {
    TempStateStore tss;
    // No persisted state → starts in Idle.
    OtaContext ctx = makeContextFromStore(tss.store());

    ctx.dispatch(OtaEvent{OtaEvent::Type::ResumeAfterBoot, "", ""});

    EXPECT_EQ(ctx.getStatus().state, OtaState::Idle);
}
