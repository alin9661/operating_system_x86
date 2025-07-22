cmake_minimum_required(VERSION 3.20)
project(ModernOS VERSION 1.0.0 LANGUAGES CXX ASM_NASM)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use toolchain file for cross-compilation (can be overridden)
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)
endif()

# Find NASM assembler
find_program(CMAKE_ASM_NASM_COMPILER NAMES nasm)
if(NOT CMAKE_ASM_NASM_COMPILER)
    message(FATAL_ERROR "NASM assembler not found. Please install nasm.")
endif()

# Additional compiler flags for kernel development
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")

# Remove global linker flags - set per target instead

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files for 64-bit kernel
set(KERNEL_SOURCES
    kernel/entry64.asm
    kernel/kernel_main64.cpp
    kernel/memory.cpp
    kernel/interrupt.cpp
    kernel/filesystem.cpp
    kernel/process.cpp
)

# Header files
set(KERNEL_HEADERS
    include/memory.h
    include/interrupt.h
    include/filesystem.h
    include/process.h
)

# Create 64-bit kernel executable
add_executable(kernel ${KERNEL_SOURCES} ${KERNEL_HEADERS})

# Set kernel-specific properties for 64-bit
set_target_properties(kernel PROPERTIES
    OUTPUT_NAME kernel.bin
    LINK_FLAGS "-T \"${CMAKE_SOURCE_DIR}/linker.ld\" -nostdlib -nostartfiles -z noexecstack"
)

# Add boot subdirectory for UEFI bootloader
add_subdirectory(boot)

# OS image creation
add_custom_target(os-image
    COMMAND ${CMAKE_COMMAND} -E echo "Creating OS image..."
    DEPENDS kernel
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating bootable OS image"
)

# Testing support (disabled for now due to cross-compilation issues)
# enable_testing()
# add_subdirectory(tests)

# Installation rules
install(TARGETS kernel
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Custom targets for development
add_custom_target(run
    COMMAND qemu-system-x86_64 
        -bios /usr/share/ovmf/OVMF.fd
        -drive file=${CMAKE_BINARY_DIR}/esp.img,format=raw,if=ide
        -m 256M -smp 1
        -serial stdio
        -no-reboot -no-shutdown
    DEPENDS esp_image
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running OS in QEMU with UEFI"
)

add_custom_target(run-legacy
    COMMAND qemu-system-x86_64 -drive file=os.img,format=raw,if=floppy
    DEPENDS os-image
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running OS in QEMU with legacy BIOS"
)

add_custom_target(debug
    COMMAND qemu-system-x86_64 
        -bios /usr/share/ovmf/OVMF.fd
        -drive file=${CMAKE_BINARY_DIR}/esp.img,format=raw,if=ide
        -m 256M -smp 1
        -serial stdio
        -no-reboot -no-shutdown
        -s -S
    DEPENDS esp_image
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running OS in QEMU with UEFI and GDB debugging"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all build files"
)

# Print configuration summary
message(STATUS "=== ModernOS Build Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "ASM Compiler: ${CMAKE_ASM_NASM_COMPILER}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source Directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "Binary Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "===================================")