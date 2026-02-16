# Tests


add_executable(cpu_tests
			cpu_tests.cpp
)

target_sources(cpu_tests PRIVATE
			${CMAKE_SOURCE_DIR}/src/emu/cpu6502/cpu.cpp
			${CMAKE_SOURCE_DIR}/src/emu/memory/memorymanager.cpp
)

target_link_libraries(cpu_tests GTest::gtest_main)

set_property(TARGET cpu_tests PROPERTY CXX_STANDARD 26)

include(GoogleTest)
gtest_discover_tests(cpu_tests)
