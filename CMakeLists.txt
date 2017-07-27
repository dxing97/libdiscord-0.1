cmake_minimum_required(VERSION 3.7)
project(libdiscord)

set(LD_VERSION_MAJOR 0)
set(LD_VERSION_MINOR 0)
set(LD_VERSION_PATCH 0)

configure_file(
        "${PROJECT_SOURCE_DIR}/libdiscordConfig.h.in"
        "${PROJECT_BINARY_DIR}/libdiscordConfig.h"
)

include_directories("${PROJECT_BINARY_DIR}")

set(LIBS websockets ulfius yder orcania jansson uv)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
        discord.c
#        test.c
        libdiscord.h libdiscord.c libdiscordConfig.h.in html.c html.h websockets.c websockets.h json.c json.h)
add_executable(libdiscord ${SOURCE_FILES})
#add_executable(testecho test-echo2.c)
target_link_libraries(libdiscord ${LIBS})