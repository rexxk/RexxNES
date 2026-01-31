# RexxNES 2026

cmake_minimum_required(VERSION 3.15)

include(FetchContent)
include(GoogleTest)

enable_testing()

FetchContent_Declare(googletest GIT_REPOSITORY http://github.com/google/googletest GIT_TAG main)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(glfw GIT_REPOSITORY http://github.com/glfw/glfw GIT_TAG master)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui GIT_REPOSITORY http://github.com/ocornut/imgui GIT_TAG docking)
FetchContent_MakeAvailable(imgui)

cmake_policy(SET CMP0076 NEW)
cmake_policy(SET CMP0079 NEW)

# imgui static library
add_library(imgui ${imgui_SOURCE_DIR}/imgui.cpp
				  ${imgui_SOURCE_DIR}/imgui_demo.cpp
				  ${imgui_SOURCE_DIR}/imgui_draw.cpp
				  ${imgui_SOURCE_DIR}/imgui_tables.cpp
				  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
				  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
				  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PRIVATE ${imgui_SOURCE_DIR} ${glfw_SOURCE_DIR}/include)

# glad static library
set (glad_SOURCE_DIR "vendor/glad")
add_library(glad ${glad_SOURCE_DIR}/src/glad.c)
target_include_directories(glad PRIVATE ${glad_SOURCE_DIR}/include)

# project

project("RexxNES")

add_executable(RexxNES)

set_property(TARGET RexxNES PROPERTY CXX_STANDARD 26)
set_property(TARGET RexxNES PROPERTY DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_subdirectory(src)

include_directories(TARGET RexxNES "${CMAKE_SOURCE_DIR}/src")
target_link_directories(RexxNES PRIVATE glfw imgui)
target_link_libraries(RexxNES PRIVATE glfw imgui)

add_subdirectory(tests)
