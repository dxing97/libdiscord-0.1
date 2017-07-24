cmake_minimum_required(VERSION 3.7)
project(libdiscord)

set(LIBS websockets ulfius yder orcania jansson)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
        discord.c
#        test.c
        libdiscord.h libdiscord.c)
add_executable(libdiscord ${SOURCE_FILES})
#add_executable(testecho test-echo2.c)
target_link_libraries(libdiscord ${LIBS})