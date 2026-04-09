cmake_minimum_required(VERSION 3.16)
project(rauc-cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(GLIB2    REQUIRED glib-2.0)
pkg_check_modules(GIO2     REQUIRED gio-2.0)
pkg_check_modules(GIOUNIX  REQUIRED gio-unix-2.0)
pkg_check_modules(CURL     REQUIRED libcurl)
pkg_check_modules(DBUS     REQUIRED dbus-1)

option(ENABLE_NETWORK   "Enable network/HTTP bundle streaming" OFF)
option(ENABLE_STREAMING "Enable HTTP streaming support"        OFF)
option(ENABLE_SERVICE   "Enable D-Bus service daemon"          ON)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

set(RAUC_SOURCES
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
    src/update_handler.cpp
    src/mount.cpp
    src/mark.cpp
    src/status_file.cpp
    src/utils.cpp
)

if(ENABLE_NETWORK)
    list(APPEND RAUC_SOURCES src/network.cpp)
endif()

if(ENABLE_SERVICE)
    list(APPEND RAUC_SOURCES src/service.cpp)
endif()

add_executable(rauc ${RAUC_SOURCES})

target_include_directories(rauc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GLIB2_INCLUDE_DIRS}
    ${GIO2_INCLUDE_DIRS}
    ${GIOUNIX_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(rauc PRIVATE
    ${GLIB2_LIBRARIES}
    ${GIO2_LIBRARIES}
    ${GIOUNIX_LIBRARIES}
    ${CURL_LIBRARIES}
    ${DBUS_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

target_compile_definitions(rauc PRIVATE
    $<$<BOOL:${ENABLE_NETWORK}>:ENABLE_NETWORK=1>
    $<$<BOOL:${ENABLE_STREAMING}>:ENABLE_STREAMING=1>
    $<$<BOOL:${ENABLE_SERVICE}>:ENABLE_SERVICE=1>
)

install(TARGETS rauc RUNTIME DESTINATION bin)
