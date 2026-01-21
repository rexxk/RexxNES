# RexxNES 2026

cmake_minimum_required(VERSION 3.15)

include(FetchContent)



cmake_policy(SET CMP0076 NEW)
cmake_policy(SET CMP0079 NEW)

# project

project("RexxNES")

add_executable(RexxNES)

set_property(TARGET RexxNES PROPERTY CXX_STANDARD 26)
set_property(TARGET RexxNES PROPERTY DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_subdirectory(src)

include_directories(TARGET RexxNES "${CMAKE_SOURCE_DIR}/src")


