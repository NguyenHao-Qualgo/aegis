cmake_minimum_required(VERSION 3.16)
project(aegis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AEGIS_ENABLE_DBUS "Build DBus daemon/client support" ON)
option(AEGIS_BUILD_TESTS "Build unit tests" ON)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(EFIVAR IMPORTED_TARGET efivar)

if (AEGIS_ENABLE_DBUS)
    find_package(sdbus-c++ REQUIRED)
endif()

add_executable(aegis
    src/main.cpp
    src/core/application.cpp
    src/common/config.cpp
    src/core/types.cpp
    src/common/util.cpp
    src/common/command_runner.cpp
    src/common/signal_set.cpp
    src/bootloader/uboot_control.cpp
    src/bootloader/nvboot_control.cpp
    src/common/state_store.cpp
    src/core/ota_context.cpp
    src/core/ota_state_machine.cpp
    src/core/ota_service.cpp
    src/stub/gcs_stub.cpp
    src/states/idle_state.cpp
    src/states/download_state.cpp
    src/states/install_state.cpp
    src/states/reboot_state.cpp
    src/states/commit_state.cpp
    src/states/failure_state.cpp
    src/installer/packer.cpp
    src/installer/installer.cpp
    src/installer/install_signal_scope.cpp
    src/installer/handler_utils.cpp
    src/installer/archive_support.cpp
    src/installer/manifest.cpp
    src/installer/raw_handler.cpp
    src/installer/archive_handler.cpp
    src/common/cpio.cpp
    src/common/crypto.cpp
    src/common/io.cpp
    src/common/logging.cpp
)

if (AEGIS_ENABLE_DBUS)
    target_sources(aegis PRIVATE
        src/core/dbus_service.cpp
        src/core/cli.cpp
    )
endif()

target_include_directories(aegis PRIVATE
    include
)

target_link_libraries(aegis PRIVATE
    PkgConfig::LIBARCHIVE
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    spdlog::spdlog_header_only
    fmt::fmt
)

add_compile_definitions(SPDLOG_FMT_EXTERNAL)

if (AEGIS_ENABLE_DBUS)
    target_link_libraries(aegis PRIVATE
        SDBusCpp::sdbus-c++
    )
    target_compile_definitions(aegis PRIVATE AEGIS_ENABLE_DBUS=1)
endif()

if (AEGIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS aegis RUNTIME DESTINATION bin)
install(FILES dbus/de.skytrack.Aegis1.xml DESTINATION share/dbus-1/interfaces)
install(FILES dbus/de.skytrack.Aegis.conf DESTINATION share/dbus-1/system.d)
install(FILES systemd/aegis.service DESTINATION lib/systemd/system)
