cmake_minimum_required(VERSION 3.5)
project(daxe)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES daxe.c)
add_executable(daxe ${SOURCE_FILES})