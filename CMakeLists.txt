cmake_minimum_required(VERSION 4.0)
project(wallpaperd)

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE WD_SOURCES src/*.c)

add_executable(wallpaperd ${WD_SOURCES})

target_include_directories(wallpaperd PRIVATE src)

