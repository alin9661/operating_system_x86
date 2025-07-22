#!/bin/bash

# ModernOS Build Script
# This script handles the build process for the modern C++ operating system with UEFI support

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check dependencies
check_dependencies() {
    print_status "Checking build dependencies..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Please install CMake 3.20 or later."
        exit 1
    fi
    
    # Check CMake version
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    if [[ $(echo "$CMAKE_VERSION 3.20" | tr ' ' '\n' | sort -V | head -n1) != "3.20" ]]; then
        print_error "CMake version 3.20 or later required. Found: $CMAKE_VERSION"
        exit 1
    fi
    
    # Check for cross-compiler
    if ! command -v x86_64-elf-gcc &> /dev/null && ! command -v x86_64-linux-gnu-gcc &> /dev/null; then
        print_warning "Cross-compiler not found. Attempting to install..."
        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y gcc-x86-64-linux-gnu g++-x86-64-linux-gnu
        elif command -v brew &> /dev/null; then
            brew install x86_64-elf-gcc
        else
            print_error "Please install x86_64-elf-gcc or x86_64-linux-gnu-gcc manually."
            exit 1
        fi
    fi
    
    # Check for UEFI development tools
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        print_warning "UEFI development tools not found. UEFI bootloader will not be built."
        print_warning "Install mingw-w64 for UEFI support."
    fi
    
    # Check for GNU-EFI
    if ! pkg-config --exists gnu-efi 2>/dev/null; then
        print_warning "GNU-EFI not found. UEFI bootloader will not be built."
        print_warning "Install gnu-efi package for UEFI support."
    fi
    
    # Check for OVMF
    if [ ! -f "/usr/share/ovmf/OVMF.fd" ] && [ ! -f "/usr/share/qemu/OVMF.fd" ]; then
        print_warning "OVMF firmware not found. Install ovmf package for UEFI testing."
    fi
    
    # Check for NASM
    if ! command -v nasm &> /dev/null; then
        print_warning "NASM not found. Attempting to install..."
        if command -v apt-get &> /dev/null; then
            sudo apt-get install -y nasm
        elif command -v brew &> /dev/null; then
            brew install nasm
        else
            print_error "Please install NASM manually."
            exit 1
        fi
    fi
    
    # Check for QEMU (optional)
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        print_warning "QEMU not found. Install QEMU to run the OS."
    fi
    
    print_success "All dependencies checked."
}

# Build function
build() {
    local BUILD_TYPE=${1:-Debug}
    local BUILD_DIR="build-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"
    
    print_status "Building ModernOS ($BUILD_TYPE)..."
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure
    print_status "Configuring build system..."
    cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
             -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    
    # Build
    print_status "Compiling kernel..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    cd ..
    print_success "Build completed successfully!"
}

# Clean function
clean() {
    print_status "Cleaning build directories..."
    rm -rf build-*
    print_success "Clean completed."
}

# Run function
run() {
    local BUILD_TYPE=${1:-Debug}
    local BUILD_DIR="build-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"
    local BOOT_MODE=${2:-uefi}
    
    if [ ! -f "$BUILD_DIR/kernel.bin" ]; then
        print_error "Kernel binary not found. Please build first."
        exit 1
    fi
    
    if ! command -v qemu-system-x86_64 &> /dev/null; then
        print_error "QEMU not found. Please install QEMU to run the OS."
        exit 1
    fi
    
    cd "$BUILD_DIR"
    
    if [ "$BOOT_MODE" = "uefi" ] && [ -f "esp.img" ]; then
        print_status "Starting ModernOS in QEMU with UEFI boot..."
        
        # Try different OVMF locations
        OVMF_PATH=""
        if [ -f "/usr/share/ovmf/OVMF.fd" ]; then
            OVMF_PATH="/usr/share/ovmf/OVMF.fd"
        elif [ -f "/usr/share/qemu/OVMF.fd" ]; then
            OVMF_PATH="/usr/share/qemu/OVMF.fd"
        elif [ -f "/opt/homebrew/share/qemu/edk2-x86_64-code.fd" ]; then
            OVMF_PATH="/opt/homebrew/share/qemu/edk2-x86_64-code.fd"
        else
            print_warning "OVMF firmware not found. Falling back to legacy boot."
            BOOT_MODE="legacy"
        fi
        
        if [ "$BOOT_MODE" = "uefi" ]; then
            qemu-system-x86_64 \
                -bios "$OVMF_PATH" \
                -drive file=esp.img,format=raw,if=ide \
                -m 256M -smp 1 \
                -serial stdio \
                -no-reboot -no-shutdown
            cd ..
            return
        fi
    fi
    
    # Legacy boot fallback
    print_status "Starting ModernOS in QEMU with legacy boot..."
    if [ -f "os.img" ]; then
        qemu-system-x86_64 -drive file=os.img,format=raw,if=floppy -m 256M -no-reboot -no-shutdown
    else
        qemu-system-x86_64 -kernel kernel.bin -m 256M -no-reboot -no-shutdown
    fi
    
    cd ..
}

# Debug function
debug() {
    local BUILD_TYPE=${1:-Debug}
    local BUILD_DIR="build-$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"
    local BOOT_MODE=${2:-uefi}
    
    if [ ! -f "$BUILD_DIR/kernel.bin" ]; then
        print_error "Kernel binary not found. Please build first."
        exit 1
    fi
    
    print_status "Starting ModernOS in QEMU with GDB debugging..."
    print_status "Connect with: gdb $BUILD_DIR/kernel.bin -ex 'target remote localhost:1234'"
    
    cd "$BUILD_DIR"
    
    if [ "$BOOT_MODE" = "uefi" ] && [ -f "esp.img" ]; then
        # Try different OVMF locations
        OVMF_PATH=""
        if [ -f "/usr/share/ovmf/OVMF.fd" ]; then
            OVMF_PATH="/usr/share/ovmf/OVMF.fd"
        elif [ -f "/usr/share/qemu/OVMF.fd" ]; then
            OVMF_PATH="/usr/share/qemu/OVMF.fd"
        elif [ -f "/opt/homebrew/share/qemu/edk2-x86_64-code.fd" ]; then
            OVMF_PATH="/opt/homebrew/share/qemu/edk2-x86_64-code.fd"
        else
            print_warning "OVMF firmware not found. Falling back to legacy boot."
            BOOT_MODE="legacy"
        fi
        
        if [ "$BOOT_MODE" = "uefi" ]; then
            qemu-system-x86_64 \
                -bios "$OVMF_PATH" \
                -drive file=esp.img,format=raw,if=ide \
                -m 256M -smp 1 \
                -serial stdio \
                -no-reboot -no-shutdown \
                -s -S
            cd ..
            return
        fi
    fi
    
    # Legacy debug fallback
    if [ -f "os.img" ]; then
        qemu-system-x86_64 -drive file=os.img,format=raw,if=floppy -m 256M -s -S -no-reboot -no-shutdown
    else
        qemu-system-x86_64 -kernel kernel.bin -m 256M -s -S -no-reboot -no-shutdown
    fi
    
    cd ..
}

# Help function
show_help() {
    echo "ModernOS Build Script (UEFI-enabled)"
    echo ""
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  build [Debug|Release]       Build the operating system (default: Debug)"
    echo "  clean                       Clean all build directories"
    echo "  run [Debug|Release] [mode]  Run the OS in QEMU (default: Debug, uefi)"
    echo "  debug [Debug|Release] [mode] Run the OS in QEMU with GDB debugging"
    echo "  deps                        Check and install dependencies"
    echo "  help                        Show this help message"
    echo ""
    echo "Boot modes:"
    echo "  uefi                        Boot with UEFI (default, requires OVMF)"
    echo "  legacy                      Boot with legacy BIOS"
    echo ""
    echo "Features:"
    echo "  - 64-bit x86_64 kernel with C++20"
    echo "  - UEFI bootloader with modern boot process"
    echo "  - Legacy BIOS fallback support"
    echo "  - Memory management with UEFI memory map"
    echo "  - Modern interrupt handling"
    echo "  - Process management with coroutines"
    echo "  - Advanced filesystem support"
    echo ""
    echo "Examples:"
    echo "  $0 build              # Build debug version"
    echo "  $0 build Release      # Build release version"
    echo "  $0 run                # Run debug version with UEFI"
    echo "  $0 run Debug legacy   # Run debug version with legacy BIOS"
    echo "  $0 debug              # Debug with UEFI boot"
    echo "  $0 clean              # Clean all builds"
    echo ""
    echo "Dependencies:"
    echo "  Core: cmake, nasm, x86_64-linux-gnu-gcc"
    echo "  UEFI: mingw-w64, gnu-efi, ovmf"
    echo "  Testing: qemu-system-x86_64"
    echo ""
}

# Main script logic
case "${1:-build}" in
    build)
        check_dependencies
        build "${2:-Debug}"
        ;;
    clean)
        clean
        ;;
    run)
        run "${2:-Debug}" "${3:-uefi}"
        ;;
    debug)
        debug "${2:-Debug}" "${3:-uefi}"
        ;;
    deps)
        check_dependencies
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        print_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac