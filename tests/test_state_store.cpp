#include <gtest/gtest.h>
#include "aegis/common/state_store.hpp"
#include <filesystem>

using namespace aegis;

class StateStoreTest : public ::testing::Test {
protected:
    const std::string path{"/tmp/aegis_unittest_store_12345.env"};

    void TearDown() override { std::filesystem::remove(path); }
};

TEST_F(StateStoreTest, LoadDefaultWhenNoFile) {
    StateStore store(path);
    const auto s = store.load();
    EXPECT_EQ(s.state, OtaState::Idle);
    EXPECT_EQ(s.progress, 0);
    EXPECT_TRUE(s.operation.empty() || s.operation == "idle");
}

TEST_F(StateStoreTest, SaveLoadRoundTrip) {
    StateStore store(path);
    OtaStatus s;
    s.state         = OtaState::Download;
    s.operation     = "downloading";
    s.progress      = 42;
    s.message       = "fetching bundle";
    s.lastError     = "";
    s.bootedSlot    = "B";
    s.primarySlot   = "A";
    s.bundleVersion = "2.5.0";
    store.save(s);

    const auto loaded = store.load();
    EXPECT_EQ(loaded.state,         OtaState::Download);
    EXPECT_EQ(loaded.operation,     "downloading");
    EXPECT_EQ(loaded.progress,      42);
    EXPECT_EQ(loaded.message,       "fetching bundle");
    EXPECT_EQ(loaded.bootedSlot,    "B");
    EXPECT_EQ(loaded.primarySlot,   "A");
    EXPECT_EQ(loaded.bundleVersion, "2.5.0");
}

TEST_F(StateStoreTest, AllOtaStatesRoundTrip) {
    StateStore store(path);
    for (auto state : {OtaState::Idle, OtaState::Download, OtaState::Install,
                       OtaState::Reboot, OtaState::Commit, OtaState::Failure}) {
        OtaStatus s;
        s.state = state;
        store.save(s);
        EXPECT_EQ(store.load().state, state) << "Failed for state: " << toString(state);
    }
}

TEST_F(StateStoreTest, SaveAndLoadError) {
    StateStore store(path);
    OtaStatus s;
    s.state     = OtaState::Failure;
    s.lastError = "signature verification failed";
    store.save(s);

    const auto loaded = store.load();
    EXPECT_EQ(loaded.state,     OtaState::Failure);
    EXPECT_EQ(loaded.lastError, "signature verification failed");
}

TEST_F(StateStoreTest, OverwritePreviousState) {
    StateStore store(path);
    OtaStatus s1;
    s1.state = OtaState::Download;
    store.save(s1);

    OtaStatus s2;
    s2.state = OtaState::Install;
    store.save(s2);

    EXPECT_EQ(store.load().state, OtaState::Install);
}

TEST_F(StateStoreTest, SaveCreatesFile) {
    EXPECT_FALSE(std::filesystem::exists(path));
    StateStore store(path);
    store.save(OtaStatus{});
    EXPECT_TRUE(std::filesystem::exists(path));
}
