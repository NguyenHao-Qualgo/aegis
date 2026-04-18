cmake_minimum_required(VERSION 3.16)

# Prefer a system-installed GTest (e.g. provided by Yocto).
# Fall back to FetchContent for local development only.
find_package(GTest QUIET)

# Fall back to FetchContent when the system package is missing or does not
# bundle GMock (common on Debian/Ubuntu where libgmock-dev is separate).
if (NOT GTest_FOUND OR NOT TARGET GTest::gmock)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()


if (TARGET GTest::GTest AND NOT TARGET GTest::gtest)
    add_library(GTest::gtest ALIAS GTest::GTest)
endif()
if (TARGET GTest::Main AND NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main ALIAS GTest::Main)
endif()

add_library(aegis_core STATIC
    ../src/config.cpp
    ../src/types.cpp
    ../src/util.cpp
    ../src/command_runner.cpp
    ../src/boot_control.cpp
    ../src/state_store.cpp
    ../src/bundle_manifest.cpp
    ../src/bundle_verifier.cpp
    ../src/bundle_extractor.cpp
    ../src/archive_update_handler.cpp
    ../src/raw_update_handler.cpp
    ../src/ota_context.cpp
    ../src/ota_state_machine.cpp
    ../src/ota_service.cpp
    ../src/gcs_stub.cpp
    ../src/states/idle_state.cpp
    ../src/states/download_state.cpp
    ../src/states/install_state.cpp
    ../src/states/reboot_state.cpp
    ../src/states/commit_state.cpp
    ../src/states/failure_state.cpp
)

target_include_directories(aegis_core PUBLIC
    ../include
    ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(aegis_core PUBLIC
    ${LIBARCHIVE_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_features(aegis_core PUBLIC cxx_std_17)

# ---- state machine tests ----
add_executable(ota_state_machine_test
    ota_state_machine_test.cpp
)

target_link_libraries(ota_state_machine_test PRIVATE
    aegis_core
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(ota_state_machine_test DISCOVERY_MODE PRE_TEST)
