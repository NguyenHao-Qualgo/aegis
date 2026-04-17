cmake_minimum_required(VERSION 3.16)
project(aegis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
find_package(sdbus-c++ REQUIRED)

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
    src/ext4_installer.cpp
    src/ota_service.cpp
    src/dbus_service.cpp
    src/client.cpp
)

target_include_directories(aegis PRIVATE
    include
    ${LIBARCHIVE_INCLUDE_DIRS}
)

target_link_libraries(aegis PRIVATE
    ${LIBARCHIVE_LIBRARIES}
    SDBusCpp::sdbus-c++
)

install(TARGETS aegis RUNTIME DESTINATION bin)
install(FILES dbus/de.skytrack.Aegis1.xml DESTINATION share/dbus-1/interfaces)
install(FILES dbus/de.skytrack.Aegis.conf DESTINATION share/dbus-1/system.d)
