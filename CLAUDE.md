# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

**Primary build command (CMake):**
```bash
./build.sh build
```

**Build and run with UEFI:**
```bash
./build.sh build && ./build.sh run
```

**Build and run with legacy BIOS:**
```bash
./build.sh build && ./build.sh run Debug legacy
```

**Clean build artifacts:**
```bash
./build.sh clean
```

**Legacy Makefile support:**
```bash
make  # Still available for compatibility
```

## Cross-Compilation Setup

This project requires cross-compilation toolchain for x86_64 target:

### Core Requirements:
- `x86_64-linux-gnu-gcc` and `x86_64-linux-gnu-g++` for kernel compilation
- `nasm` for assembly
- `cmake` 3.20+ for build system
- `qemu-system-x86_64` for emulation

### UEFI Development (Optional but Recommended):
- `x86_64-w64-mingw32-gcc` for UEFI bootloader compilation
- `gnu-efi` library for UEFI development
- `ovmf` firmware for UEFI testing in QEMU
- `mtools` for FAT filesystem manipulation

### Installation:
```bash
# Ubuntu/Debian
sudo apt-get install cmake nasm gcc-x86-64-linux-gnu qemu-system-x86
sudo apt-get install mingw-w64 gnu-efi ovmf mtools

# Arch Linux
sudo pacman -S cmake nasm x86_64-linux-gnu-gcc qemu-system-x86
sudo pacman -S mingw-w64-gcc gnu-efi ovmf mtools
```

## Architecture Overview

### Boot Process
ModernOS supports two boot methods:

**UEFI Boot (Recommended):**
- **UEFI Bootloader** (`boot/uefi_bootloader.c`): Modern UEFI application
- **64-bit Entry** (`kernel/entry64.asm`): Assembly entry point for long mode
- **Kernel Main** (`kernel/kernel_main64.cpp`): C++ kernel entry at higher half

**Legacy BIOS Boot (Fallback):**
- **Legacy Bootloader** (`boot/boot.asm`): 16-bit assembly bootloader
- **Kernel Entry** (`kernel/kernel_main64.cpp`): C++ kernel entry

### Memory Layout (64-bit)
- **Higher Half Kernel**: Virtual base at `0xFFFFFFFF80000000`
- **Physical Kernel**: Loaded at `0x100000` (1MB)
- **Stack**: 1MB allocated stack space
- **Heap**: 16MB initial heap allocation
- **UEFI Memory Map**: Dynamic memory management from firmware

### Kernel Architecture
The kernel follows a modern modular design with C++20 features:

**Memory Management** (`kernel/memory.cpp`, `include/memory.h`):
- C++20 concepts and smart pointers
- UEFI memory map integration
- Page-based allocation (4KB pages)
- Physical/virtual memory mapping with higher half
- std::expected for error handling
- Memory statistics and tracking

**Process Management** (`kernel/process.cpp`, `include/process.h`):
- C++20 coroutines for cooperative multitasking
- Advanced process states and scheduling classes
- RAII process handles
- Capabilities-based security model
- Resource limits and accounting
- Modern CPU context management (x86_64)

**Interrupt Handling** (`kernel/interrupt.cpp`, `include/interrupt.h`):
- 64-bit IDT with proper exception handling
- RAII interrupt guards
- Modern interrupt controller support (PIC/APIC)
- Performance monitoring and latency measurement
- Exception handlers with detailed reporting

**File System** (`kernel/filesystem.cpp`, `include/filesystem.h`):
- Modern filesystem interface with concepts
- POSIX-style permissions and metadata
- Path manipulation utilities
- RAII file handles
- Memory-mapped files and advanced operations
- Support for multiple filesystem types

### Initialization Sequence
The kernel initializes subsystems in this order:
1. **Boot Configuration**: Parse UEFI boot parameters
2. **Console**: Initialize early VGA text output
3. **Memory Management**: `Memory::init()` with UEFI memory map
4. **Interrupt Handling**: `Interrupt::init()` with 64-bit IDT
5. **File System**: `FileSystem::init()` 
6. **Process Management**: `Process::init()` with modern scheduler
7. **Test Processes**: Run kernel functionality tests
8. **Main Loop**: Enter kernel idle loop with power management

### Build System Details

**Modern CMake Build:**
- C++20 standard with concepts, coroutines, and std::expected
- Cross-compilation for x86_64-elf target
- Separate UEFI bootloader compilation with mingw-w64
- Comprehensive testing with Google Test
- CI/CD pipeline with GitHub Actions
- Multiple build configurations (Debug/Release)

**Compilation Flags:**
- Kernel: `-ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -std=c++20`
- UEFI: `-fpic -ffreestanding -fno-stack-protector -fshort-wchar`
- Linker: Higher half kernel mapping with custom linker script

**Output Artifacts:**
- `kernel.bin`: 64-bit kernel binary
- `bootx64.efi`: UEFI bootloader (if tools available)
- `esp.img`: EFI System Partition image for UEFI boot
- `os.img`: Legacy disk image for BIOS boot

### Key Implementation Notes

**C++20 Features:**
- Concepts for type safety and better error messages
- Coroutines for cooperative multitasking
- std::expected for comprehensive error handling
- Smart pointers and RAII throughout
- Constexpr and consteval for compile-time optimization

**64-bit Architecture:**
- Long mode operation with 4-level paging
- Higher half kernel mapping (0xFFFFFFFF80000000)
- x86_64 calling conventions
- Modern CPU feature detection (SSE, AVX)
- 64-bit addressing throughout

**UEFI Integration:**
- Graphics Output Protocol (GOP) support
- UEFI memory map parsing and integration
- Runtime services access
- Modern boot process with proper handoff

**Development Features:**
- Comprehensive error handling and reporting
- Debug output through VGA text mode and serial
- QEMU integration for testing
- GDB debugging support
- Extensive logging and introspection

### Testing and Debugging

**Running the OS:**
```bash
# UEFI boot (recommended)
./build.sh run

# Legacy BIOS boot
./build.sh run Debug legacy

# Debug with GDB
./build.sh debug
# In another terminal: gdb build-debug/kernel.bin -ex 'target remote :1234'
```

**Testing:**
```bash
# Run unit tests (Debug build only)
./build.sh build Debug
cd build-debug && ctest --verbose
```

### Troubleshooting

**Common Issues:**
1. **UEFI tools missing**: Install mingw-w64 and gnu-efi packages
2. **OVMF not found**: Install ovmf package or specify custom path
3. **Cross-compiler not found**: Install x86_64-linux-gnu-gcc
4. **Build fails**: Ensure CMake 3.20+ and C++20 compiler support

**Platform-Specific Notes:**
- **Linux**: Full UEFI development support
- **macOS**: Limited UEFI support, legacy boot recommended
- **Windows**: Use WSL2 for best compatibility