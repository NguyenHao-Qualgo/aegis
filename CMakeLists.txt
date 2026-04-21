cmake_minimum_required(VERSION 3.16)
project(aegis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AEGIS_ENABLE_DBUS "Build DBus daemon/client support" ON)
option(AEGIS_BUILD_TESTS "Build unit tests" ON)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(EFIVAR IMPORTED_TARGET efivar)

if (AEGIS_ENABLE_DBUS)
    find_package(sdbus-c++ REQUIRED)
endif()

add_executable(aegis
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/types.cpp
    src/util.cpp
    src/command_runner.cpp
    src/bootloader/uboot_control.cpp
    src/bootloader/nvboot_control.cpp
    src/state_store.cpp
    src/ota_context.cpp
    src/ota_state_machine.cpp
    src/ota_service.cpp
    src/gcs_stub.cpp
    src/states/idle_state.cpp
    src/states/download_state.cpp
    src/states/install_state.cpp
    src/states/reboot_state.cpp
    src/states/commit_state.cpp
    src/states/failure_state.cpp
    src/packer.cpp
    src/installer.cpp
    src/manifest.cpp
    src/raw_handler.cpp
    src/archive_handler.cpp
    src/cpio.cpp
    src/crypto.cpp
    src/installer.cpp
    src/io.cpp
    src/manifest.cpp
    src/packer.cpp
    src/raw_handler.cpp
)

if (AEGIS_ENABLE_DBUS)
    target_sources(aegis PRIVATE
        src/dbus_service.cpp
        src/cli.cpp
    )
endif()

target_include_directories(aegis PRIVATE
    include
)

target_link_libraries(aegis PRIVATE
    PkgConfig::LIBARCHIVE
    OpenSSL::SSL
    OpenSSL::Crypto
)

if (AEGIS_ENABLE_DBUS)
    target_link_libraries(aegis PRIVATE
        SDBusCpp::sdbus-c++
    )
    target_compile_definitions(aegis PRIVATE AEGIS_ENABLE_DBUS=1)
endif()

install(TARGETS aegis RUNTIME DESTINATION bin)
install(FILES dbus/de.skytrack.Aegis1.xml DESTINATION share/dbus-1/interfaces)
install(FILES dbus/de.skytrack.Aegis.conf DESTINATION share/dbus-1/system.d)
install(FILES systemd/aegis.service DESTINATION lib/systemd/system)
