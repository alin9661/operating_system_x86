# Boot directory CMake configuration for UEFI bootloader

# Check if we have the required tools for UEFI development
find_program(MINGW_GCC x86_64-w64-mingw32-gcc)
find_program(MINGW_LD x86_64-w64-mingw32-ld)
find_program(OBJCOPY x86_64-w64-mingw32-objcopy)

if(NOT MINGW_GCC OR NOT MINGW_LD OR NOT OBJCOPY)
    message(WARNING "UEFI development tools not found. UEFI bootloader will not be built.")
    message(STATUS "Required tools: x86_64-w64-mingw32-gcc, x86_64-w64-mingw32-ld, x86_64-w64-mingw32-objcopy")
    return()
endif()

# Try to find GNU-EFI
find_path(GNU_EFI_INCLUDE_DIR
    NAMES efi.h
    PATHS 
        /usr/include/efi
        /usr/local/include/efi
        /opt/gnu-efi/include/efi
        ${CMAKE_SOURCE_DIR}/external/gnu-efi/include/efi
)

find_path(GNU_EFI_INCLUDE_X86_64_DIR
    NAMES efibind.h
    PATHS 
        /usr/include/efi/x86_64
        /usr/local/include/efi/x86_64
        /opt/gnu-efi/include/efi/x86_64
        ${CMAKE_SOURCE_DIR}/external/gnu-efi/include/efi/x86_64
)

find_library(GNU_EFI_LIB
    NAMES efi
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/gnu-efi/lib
        ${CMAKE_SOURCE_DIR}/external/gnu-efi/lib
)

find_library(GNU_GNUEFI_LIB
    NAMES gnuefi
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/gnu-efi/lib
        ${CMAKE_SOURCE_DIR}/external/gnu-efi/lib
)

find_file(GNU_EFI_CRT0
    NAMES crt0-efi-x86_64.o
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/gnu-efi/lib
        ${CMAKE_SOURCE_DIR}/external/gnu-efi/lib
)

if(NOT GNU_EFI_INCLUDE_DIR OR NOT GNU_EFI_INCLUDE_X86_64_DIR OR 
   NOT GNU_EFI_LIB OR NOT GNU_GNUEFI_LIB OR NOT GNU_EFI_CRT0)
    message(WARNING "GNU-EFI not found. Please install gnu-efi development package.")
    message(STATUS "  On Ubuntu/Debian: sudo apt-get install gnu-efi")
    message(STATUS "  On RHEL/CentOS: sudo yum install gnu-efi-devel")
    message(STATUS "  On Arch: sudo pacman -S gnu-efi")
    return()
endif()

message(STATUS "Found GNU-EFI:")
message(STATUS "  Include dir: ${GNU_EFI_INCLUDE_DIR}")
message(STATUS "  x86_64 include dir: ${GNU_EFI_INCLUDE_X86_64_DIR}")
message(STATUS "  EFI library: ${GNU_EFI_LIB}")
message(STATUS "  GNU-EFI library: ${GNU_GNUEFI_LIB}")
message(STATUS "  CRT0 object: ${GNU_EFI_CRT0}")

# UEFI bootloader target
add_custom_target(uefi_bootloader ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bootx64.efi
)

# UEFI bootloader compilation
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bootx64.efi
    COMMAND ${MINGW_GCC}
        -I${GNU_EFI_INCLUDE_DIR}
        -I${GNU_EFI_INCLUDE_X86_64_DIR}
        -fpic -ffreestanding -fno-stack-protector -fno-stack-check
        -fshort-wchar -mno-red-zone -maccumulate-outgoing-args
        -c ${CMAKE_CURRENT_SOURCE_DIR}/uefi_bootloader.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/uefi_bootloader.o
    COMMAND ${MINGW_LD}
        -nostdlib -znocombreloc -T ${CMAKE_CURRENT_SOURCE_DIR}/uefi.lds
        -shared -Bsymbolic -L/usr/lib -L/usr/local/lib
        ${GNU_EFI_CRT0}
        ${CMAKE_CURRENT_BINARY_DIR}/uefi_bootloader.o
        -o ${CMAKE_CURRENT_BINARY_DIR}/bootx64.so
        -lefi -lgnuefi
    COMMAND ${OBJCOPY}
        -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel
        -j .rela -j .reloc --target=efi-app-x86_64
        ${CMAKE_CURRENT_BINARY_DIR}/bootx64.so
        ${CMAKE_CURRENT_BINARY_DIR}/bootx64.efi
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/uefi_bootloader.c
    COMMENT "Building UEFI bootloader"
    VERBATIM
)

# Create UEFI linker script
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/uefi.lds.in
    ${CMAKE_CURRENT_SOURCE_DIR}/uefi.lds
    @ONLY
)

# ESP (EFI System Partition) image creation
find_program(DD_PROGRAM dd)
find_program(MKFS_FAT mkfs.fat)
find_program(MCOPY mcopy)

if(DD_PROGRAM AND MKFS_FAT AND MCOPY)
    add_custom_target(esp_image
        COMMAND ${DD_PROGRAM} if=/dev/zero of=${CMAKE_BINARY_DIR}/esp.img bs=1M count=64
        COMMAND ${MKFS_FAT} -F 32 -n "EFI System" ${CMAKE_BINARY_DIR}/esp.img
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/esp_mount
        COMMAND ${MCOPY} -i ${CMAKE_BINARY_DIR}/esp.img -s 
                ${CMAKE_CURRENT_BINARY_DIR}/bootx64.efi ::EFI/BOOT/
        COMMAND ${MCOPY} -i ${CMAKE_BINARY_DIR}/esp.img -s 
                ${CMAKE_BINARY_DIR}/kernel.bin ::EFI/ModernOS/
        DEPENDS uefi_bootloader kernel
        COMMENT "Creating ESP image"
        VERBATIM
    )
else()
    message(WARNING "Tools for ESP image creation not found (dd, mkfs.fat, mcopy)")
endif()

# Install UEFI bootloader
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bootx64.efi
        DESTINATION boot/EFI/BOOT/)

# Legacy BIOS bootloader (keep for fallback)
if(NASM_FOUND)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/boot.bin
        COMMAND ${NASM_EXECUTABLE} -f bin 
                ${CMAKE_CURRENT_SOURCE_DIR}/boot.asm 
                -o ${CMAKE_CURRENT_BINARY_DIR}/boot.bin
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/boot.asm
        COMMENT "Building legacy BIOS bootloader"
    )
    
    add_custom_target(legacy_bootloader
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/boot.bin
    )
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/boot.bin
            DESTINATION boot/)
endif()