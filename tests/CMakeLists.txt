# Testing CMakeLists.txt for ModernOS
# This file sets up the testing infrastructure using Google Test

cmake_minimum_required(VERSION 3.20)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Create a special test-only version of kernel components
# These will compile without kernel-specific flags for testing
add_library(kernel_test_lib STATIC
    ../kernel/memory.cpp
    ../kernel/process.cpp
    ../kernel/interrupt.cpp
    ../kernel/filesystem.cpp
)

# Override compiler flags for testing (remove kernel-specific flags)
target_compile_options(kernel_test_lib PRIVATE
    -std=c++20
    -Wall
    -Wextra
    -g
    -DTESTING_MODE  # Define this to enable test-specific code paths
)

# Remove kernel-specific flags that don't work in test environment
target_compile_definitions(kernel_test_lib PRIVATE
    TESTING_MODE=1
)

target_include_directories(kernel_test_lib PRIVATE
    ../include
)

# Test executables
add_executable(memory_tests
    memory_tests.cpp
)

add_executable(process_tests
    process_tests.cpp
)

add_executable(interrupt_tests
    interrupt_tests.cpp
)

add_executable(filesystem_tests
    filesystem_tests.cpp
)

# Link test executables with Google Test and kernel library
target_link_libraries(memory_tests
    gtest_main
    kernel_test_lib
)

target_link_libraries(process_tests
    gtest_main
    kernel_test_lib
)

target_link_libraries(interrupt_tests
    gtest_main
    kernel_test_lib
)

target_link_libraries(filesystem_tests
    gtest_main
    kernel_test_lib
)

# Add include directories
target_include_directories(memory_tests PRIVATE ../include)
target_include_directories(process_tests PRIVATE ../include)
target_include_directories(interrupt_tests PRIVATE ../include)
target_include_directories(filesystem_tests PRIVATE ../include)

# Register tests with CTest
enable_testing()
add_test(NAME MemoryTests COMMAND memory_tests)
add_test(NAME ProcessTests COMMAND process_tests)
add_test(NAME InterruptTests COMMAND interrupt_tests)
add_test(NAME FilesystemTests COMMAND filesystem_tests)

# Set test properties
set_tests_properties(MemoryTests ProcessTests InterruptTests FilesystemTests
    PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS memory_tests process_tests interrupt_tests filesystem_tests
    COMMENT "Running all unit tests"
)