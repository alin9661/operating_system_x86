# Compiler and tools
ASM=nasm
CC=i686-elf-gcc
CXX=i686-elf-g++
LD=i686-elf-ld

# Flags
ASMFLAGS=-f bin
CXXFLAGS=-ffreestanding -O2 -Wall -Wextra -I./include -std=c++11 -fno-exceptions -fno-rtti
LDFLAGS=-T linker.ld -nostdlib

# Directories
BUILD_DIR=build
BOOT_DIR=boot
KERNEL_DIR=kernel
INCLUDE_DIR=include

# Source files
BOOT_SRC=$(BOOT_DIR)/boot.asm
KERNEL_SRCS=$(KERNEL_DIR)/kernel.cpp \
            $(KERNEL_DIR)/memory.cpp \
            $(KERNEL_DIR)/interrupt.cpp \
            $(KERNEL_DIR)/filesystem.cpp \
            $(KERNEL_DIR)/process.cpp

# Object files
BOOT_BIN=$(BUILD_DIR)/boot.bin
KERNEL_OBJS=$(KERNEL_SRCS:$(KERNEL_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Output files
OS_IMAGE=$(BUILD_DIR)/os.img

# Default target
all: $(BUILD_DIR) $(OS_IMAGE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build bootloader
$(BOOT_BIN): $(BOOT_SRC)
	$(ASM) $(ASMFLAGS) $< -o $@

# Build kernel objects
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link everything together
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_OBJS)
	cat $(BOOT_BIN) > $(OS_IMAGE)
	# Add kernel after bootloader
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) conv=notrunc

# Run in QEMU
run: $(OS_IMAGE)
	qemu-system-i386 -fda $(OS_IMAGE)

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean run 