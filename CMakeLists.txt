cmake_minimum_required(VERSION 3.16)
project(aegis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AEGIS_ENABLE_DBUS "Build DBus daemon/client support" ON)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
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
    src/boot_control.cpp
    src/state_store.cpp
    src/bundle_manifest.cpp
    src/bundle_creator.cpp
    src/bundle_verifier.cpp
    src/bundle_extractor.cpp
    src/archive_update_handler.cpp
    src/raw_update_handler.cpp

    src/ota_context.cpp
    src/ota_service.cpp

    src/states/idle_state.cpp
    src/states/sync_state.cpp
    src/states/verify_bundle_state.cpp
    src/states/extract_state.cpp
    src/states/install_state.cpp
    src/states/reboot_state.cpp
    src/states/commit_state.cpp
    src/states/failure_state.cpp
)

if (AEGIS_ENABLE_DBUS)
    target_sources(aegis PRIVATE
        src/dbus_service.cpp
        src/client.cpp
    )
endif()

target_include_directories(aegis PRIVATE
    include
    ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(aegis PRIVATE
    ${LIBARCHIVE_LIBRARIES}
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