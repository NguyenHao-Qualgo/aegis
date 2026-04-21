cmake_minimum_required(VERSION 3.16)

find_package(GTest REQUIRED)
include(GoogleTest)

option(AEGIS_ENABLE_COVERAGE "Enable gcov line coverage reporting" OFF)

set(AEGIS_TESTABLE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/common/util.cpp
    ${CMAKE_SOURCE_DIR}/src/core/types.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ota_context.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ota_state_machine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ota_service.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config.cpp
    ${CMAKE_SOURCE_DIR}/src/config/state_store.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/manifest.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/packer.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/installer.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/raw_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/installer/archive_handler.cpp
    ${CMAKE_SOURCE_DIR}/src/crypto/crypto.cpp
    ${CMAKE_SOURCE_DIR}/src/io/io.cpp
    ${CMAKE_SOURCE_DIR}/src/io/cpio.cpp
    ${CMAKE_SOURCE_DIR}/src/io/command_runner.cpp
    ${CMAKE_SOURCE_DIR}/src/service/gcs_stub.cpp
    ${CMAKE_SOURCE_DIR}/src/bootloader/uboot_control.cpp
    ${CMAKE_SOURCE_DIR}/src/bootloader/nvboot_control.cpp
    ${CMAKE_SOURCE_DIR}/src/states/idle_state.cpp
    ${CMAKE_SOURCE_DIR}/src/states/download_state.cpp
    ${CMAKE_SOURCE_DIR}/src/states/install_state.cpp
    ${CMAKE_SOURCE_DIR}/src/states/reboot_state.cpp
    ${CMAKE_SOURCE_DIR}/src/states/commit_state.cpp
    ${CMAKE_SOURCE_DIR}/src/states/failure_state.cpp
)

add_library(aegis_testable STATIC ${AEGIS_TESTABLE_SOURCES})
target_include_directories(aegis_testable PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(aegis_testable PUBLIC
    PkgConfig::LIBARCHIVE
    OpenSSL::SSL
    OpenSSL::Crypto
)

if (AEGIS_ENABLE_COVERAGE)
    target_compile_options(aegis_testable PUBLIC
        --coverage -O0 -g -fprofile-arcs -ftest-coverage)
    target_link_options(aegis_testable PUBLIC --coverage)
endif()

function(aegis_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE aegis_testable GTest::GTest GTest::Main)
    if (AEGIS_ENABLE_COVERAGE)
        target_compile_options(${name} PRIVATE --coverage -O0 -g)
        target_link_options(${name} PRIVATE --coverage)
    endif()
    gtest_discover_tests(${name})
endfunction()

aegis_test(test_util)
aegis_test(test_types)
aegis_test(test_error)
aegis_test(test_config)
aegis_test(test_state_store)
aegis_test(test_manifest)
aegis_test(test_sha256)
aegis_test(test_crypto)
aegis_test(test_cpio)
aegis_test(test_command_runner)
aegis_test(test_packer)
aegis_test(test_ota_state_machine)

if (AEGIS_ENABLE_COVERAGE)
    add_custom_target(coverage
        COMMENT "Generating HTML coverage report in ${CMAKE_BINARY_DIR}/coverage_report/"
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -j4
        COMMAND lcov
            --capture
            --directory ${CMAKE_BINARY_DIR}
            --output-file ${CMAKE_BINARY_DIR}/coverage.info
            --exclude "*/tests/*"
            --exclude "/usr/*"
            --exclude "*/gtest*"
            --exclude "*/_deps/*"
        COMMAND genhtml
            ${CMAKE_BINARY_DIR}/coverage.info
            --output-directory ${CMAKE_BINARY_DIR}/coverage_report
        COMMAND lcov --list ${CMAKE_BINARY_DIR}/coverage.info
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
