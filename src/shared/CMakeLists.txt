cmake_minimum_required(VERSION 3.15)
project(iotshared)

# Add an option to control the installation of development files. Default is ON.
option(INSTALL_DEV_HEADERS "Install header files and CMake config for development" ON)

file(GLOB SHARED_SRC src/*.cpp)
file(GLOB SHARED_HDR inc/*.h)

add_library(iotshared SHARED ${SHARED_SRC} ${SHARED_HDR})
set_target_properties(iotshared PROPERTIES SOVERSION 1)

target_include_directories(iotshared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${BREAKPAD_INCLUDES}
)

target_link_libraries(iotshared ${SERVICE_LIBS})

if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    separate_release_and_debug(iotshared)
    generate_breakpad_symbols(iotshared)
endif()

install(TARGETS
    iotshared
    LIBRARY DESTINATION lib)

if(INSTALL_DEV_HEADERS)
    install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/inc/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

endif()