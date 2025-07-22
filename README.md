# Simple Operating System

This is a basic operating system implementation in C++. The project aims to demonstrate core operating system concepts and provide a learning platform for understanding OS development.

## Project Structure

- `boot/` - Contains bootloader code
- `kernel/` - Contains the kernel implementation
- `include/` - Header files
- `build/` - Build output directory

## Requirements

- NASM (Netwide Assembler)
- GCC/G++ (with cross-compiler support)
- Make
- QEMU (for testing)

## Building

```bash
make
```

## Running

```bash
make run
```

This will build the OS and launch it in QEMU.

## Features (Planned)

- Basic bootloader
- Kernel initialization
- Memory management
- Basic I/O operations
- Interrupt handling 