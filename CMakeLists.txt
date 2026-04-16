cmake_minimum_required(VERSION 3.16)
project(aegis-cpp VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Git QUIET)

set(AEGIS_BUILD_VERSION "${PROJECT_VERSION}")
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE AEGIS_BUILD_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

set(DBUS_SYSTEM_POLICY_DIR "/usr/share/dbus-1/system.d"
    CACHE PATH "Install path for the Aegis D-Bus system bus policy")
set(DBUS_SYSTEM_SERVICES_DIR "/usr/share/dbus-1/system-services"
    CACHE PATH "Install path for the Aegis D-Bus activation service")
set(SYSTEMD_SYSTEM_UNIT_DIR "/usr/lib/systemd/system"
    CACHE PATH "Install path for the Aegis systemd service unit")
set(AEGIS_DBUS_INTERFACES_DIR "/usr/share/dbus-1/interfaces"
    CACHE PATH "Install path for the Aegis D-Bus interface XML")
set(AEGIS_DBUS_INTROSPECTION_XML_PATH "${AEGIS_DBUS_INTERFACES_DIR}/de.pengutronix.aegis.Installer.xml"
    CACHE FILEPATH "Runtime path to the Aegis D-Bus introspection XML")

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(GLIB2    REQUIRED glib-2.0)
pkg_check_modules(GIO2     REQUIRED gio-2.0)
pkg_check_modules(GIOUNIX  REQUIRED gio-unix-2.0)
pkg_check_modules(CURL     REQUIRED libcurl)
pkg_check_modules(DBUS     REQUIRED dbus-1)
pkg_check_modules(ARCHIVE  REQUIRED libarchive)

option(ENABLE_NETWORK   "Enable network/HTTP bundle streaming" OFF)
option(ENABLE_STREAMING "Enable HTTP streaming support"        OFF)
option(ENABLE_SERVICE   "Enable D-Bus service daemon"          ON)
option(ENABLE_CLANG_TIDY "Run clang-tidy during compilation when available" OFF)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

if(ENABLE_SERVICE)
    set(bindir "${CMAKE_INSTALL_FULL_BINDIR}")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/packaging/systemd/system/aegis.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/aegis.service
        @ONLY
    )
endif()

set(AEGIS_SOURCES
    src/main.cpp
    src/context.cpp
    src/config_file.cpp
    src/manifest.cpp
    src/bundle.cpp
    src/signature.cpp
    src/checksum.cpp
    src/crypt.cpp
    src/verity_hash.cpp
    src/dm.cpp
    src/slot.cpp
    src/bootchooser.cpp
    src/install.cpp
    src/install/workflow.cpp
    src/handlers/update_handler.cpp
    src/handlers/block_handler.cpp
    src/handlers/raw_handler.cpp
    src/handlers/fs_image_handler.cpp
    src/handlers/archive_handler.cpp
    src/mount.cpp
    src/mark.cpp
    src/status_file.cpp
    src/utils.cpp
    src/cli/app_context.cpp
    src/cli/application.cpp
    src/cli/parser.cpp
    src/cli/registry.cpp
    src/cli/commands/bundle_commands.cpp
    src/cli/commands/service_commands.cpp
    src/dbus/client.cpp
    src/agent/session_store.cpp
    src/agent/state_machine.cpp
)

set(AEGIS_ALL_FILES
    ${AEGIS_SOURCES}
    include/aegis/bootchooser.h
    include/aegis/bundle.h
    include/aegis/checksum.h
    include/aegis/config_file.h
    include/aegis/context.h
    include/aegis/crypt.h
    include/aegis/dm.h
    include/aegis/error.h
    include/aegis/install.h
    include/aegis/manifest.h
    include/aegis/mark.h
    include/aegis/mount.h
    include/aegis/progress_info.h
    include/aegis/signature.h
    include/aegis/slot.h
    include/aegis/status_file.h
    include/aegis/utils.h
    include/aegis/verity_hash.h
    include/aegis/agent/session.h
    include/aegis/agent/session_store.h
    include/aegis/agent/state_machine.h
    include/aegis/cli/app_context.h
    include/aegis/cli/application.h
    include/aegis/cli/cli_options.h
    include/aegis/cli/cli_parser.h
    include/aegis/cli/command_registry.h
    include/aegis/cli/commands.h
    include/aegis/dbus/client.h
    include/aegis/dbus/interface.h
    include/aegis/dbus/message_builder.h
    include/aegis/dbus/service.h
    include/aegis/dbus/service_state.h
    include/aegis/install/workflow.h
    include/aegis/handlers/archive_handler.h
    include/aegis/handlers/block_handler.h
    include/aegis/handlers/fs_image_handler.h
    include/aegis/handlers/raw_handler.h
    include/aegis/handlers/update_handler.h
)

if(ENABLE_SERVICE)
    list(APPEND AEGIS_SOURCES
    src/dbus/service.cpp
    src/dbus/message_builder.cpp
    src/dbus/service_state.cpp
    )
endif()

add_executable(aegis ${AEGIS_SOURCES})

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXE} -i ${AEGIS_ALL_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting Aegis sources with clang-format"
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(
        tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${AEGIS_ALL_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy on Aegis sources"
    )

    if(ENABLE_CLANG_TIDY)
        set_target_properties(aegis PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-p;${CMAKE_BINARY_DIR}")
    endif()
endif()

target_include_directories(aegis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GLIB2_INCLUDE_DIRS}
    ${GIO2_INCLUDE_DIRS}
    ${GIOUNIX_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${ARCHIVE_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(aegis PRIVATE
    ${GLIB2_LIBRARIES}
    ${GIO2_LIBRARIES}
    ${GIOUNIX_LIBRARIES}
    ${CURL_LIBRARIES}
    ${DBUS_LIBRARIES}
    ${ARCHIVE_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

target_compile_definitions(aegis PRIVATE
    $<$<BOOL:${ENABLE_NETWORK}>:ENABLE_NETWORK=1>
    $<$<BOOL:${ENABLE_STREAMING}>:ENABLE_STREAMING=1>
    $<$<BOOL:${ENABLE_SERVICE}>:ENABLE_SERVICE=1>
)

install(TARGETS aegis RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(ENABLE_SERVICE)
    install(FILES packaging/dbus-1/system.d/de.pengutronix.aegis.conf
            DESTINATION ${DBUS_SYSTEM_POLICY_DIR})
    install(FILES packaging/dbus-1/introspection/de.pengutronix.aegis.Installer.xml
            DESTINATION ${AEGIS_DBUS_INTERFACES_DIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aegis.service
            DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})
endif()
