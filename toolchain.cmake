# CMake toolchain file for cross-compiling to x86_64
# This file configures the cross-compilation environment for ModernOS

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_CROSSCOMPILING TRUE)

# Specify the cross-compiler
set(CMAKE_C_COMPILER x86_64-elf-gcc)
set(CMAKE_CXX_COMPILER x86_64-elf-g++)
set(CMAKE_ASM_COMPILER x86_64-elf-gcc)
set(CMAKE_LINKER x86_64-elf-ld)
set(CMAKE_OBJCOPY x86_64-elf-objcopy)
set(CMAKE_OBJDUMP x86_64-elf-objdump)
set(CMAKE_SIZE x86_64-elf-size)
set(CMAKE_AR x86_64-elf-ar)
set(CMAKE_RANLIB x86_64-elf-ranlib)

# Where to find the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Compiler flags for kernel development
set(CMAKE_C_FLAGS_INIT "-ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib -nostartfiles")

# Assembly flags
set(CMAKE_ASM_FLAGS_INIT "-64")