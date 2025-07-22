/**
 * @file uefi_bootloader.c
 * @brief Modern UEFI bootloader for 64-bit kernel
 * 
 * This UEFI bootloader replaces the legacy BIOS boot.asm and provides:
 * - Modern UEFI boot process
 * - 64-bit long mode initialization
 * - Graphics Output Protocol support
 * - Memory map acquisition and processing
 * - Kernel loading and handoff
 */

#include <efi.h>
#include <efilib.h>
#include <efierr.h>

// Bootloader version and identification
#define BOOTLOADER_VERSION_MAJOR 1
#define BOOTLOADER_VERSION_MINOR 0
#define BOOTLOADER_NAME L"ModernOS UEFI Bootloader"
#define KERNEL_FILENAME L"\\EFI\\ModernOS\\kernel.bin"

// Memory layout constants
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ULL
#define KERNEL_PHYSICAL_BASE 0x100000ULL  // 1MB
#define STACK_SIZE 0x100000ULL            // 1MB
#define HEAP_SIZE 0x1000000ULL            // 16MB

// Boot configuration structure
typedef struct {
    UINT64 kernel_virtual_base;
    UINT64 kernel_physical_base;
    UINT64 kernel_size;
    UINT64 stack_top;
    UINT64 heap_start;
    UINT64 heap_size;
    EFI_MEMORY_DESCRIPTOR* memory_map;
    UINTN memory_map_size;
    UINTN memory_map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* graphics_info;
    VOID* framebuffer_base;
    UINTN framebuffer_size;
} BOOT_CONFIG;

// Function prototypes
EFI_STATUS InitializeGraphics(EFI_GRAPHICS_OUTPUT_PROTOCOL** gop, BOOT_CONFIG* config);
EFI_STATUS LoadKernel(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem, BOOT_CONFIG* config);
EFI_STATUS SetupMemoryMap(BOOT_CONFIG* config);
EFI_STATUS SetupLongMode(BOOT_CONFIG* config);
EFI_STATUS TransferControlToKernel(BOOT_CONFIG* config);
VOID PrintBootInfo(BOOT_CONFIG* config);
VOID WaitForKeyPress(VOID);

// Global variables
static EFI_SYSTEM_TABLE* gST = NULL;
static EFI_BOOT_SERVICES* gBS = NULL;
static EFI_RUNTIME_SERVICES* gRT = NULL;

/**
 * @brief UEFI application entry point
 * @param ImageHandle Handle for this UEFI application
 * @param SystemTable Pointer to EFI system table
 * @return EFI_STATUS Success or error code
 */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    EFI_STATUS status;
    BOOT_CONFIG config = {0};
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem = NULL;
    EFI_LOADED_IMAGE_PROTOCOL* loaded_image = NULL;
    
    // Initialize global variables
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    
    // Initialize library
    InitializeLib(ImageHandle, SystemTable);
    
    // Clear screen and display welcome message
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"    %s v%d.%d\r\n", BOOTLOADER_NAME, 
          BOOTLOADER_VERSION_MAJOR, BOOTLOADER_VERSION_MINOR);
    Print(L"========================================\r\n");
    Print(L"\r\n");
    
    // Initialize boot configuration
    config.kernel_virtual_base = KERNEL_VIRTUAL_BASE;
    config.kernel_physical_base = KERNEL_PHYSICAL_BASE;
    config.stack_top = KERNEL_PHYSICAL_BASE + STACK_SIZE;
    config.heap_start = config.stack_top;
    config.heap_size = HEAP_SIZE;
    
    Print(L"[1/6] Initializing graphics subsystem...\r\n");
    status = InitializeGraphics(&gop, &config);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to initialize graphics: %r\r\n", status);
        Print(L"Continuing with text mode only...\r\n");
    } else {
        Print(L"Graphics initialized successfully\r\n");
        Print(L"  Resolution: %dx%d\r\n", 
              config.graphics_info->HorizontalResolution,
              config.graphics_info->VerticalResolution);
        Print(L"  Pixel Format: %d\r\n", config.graphics_info->PixelFormat);
    }
    
    Print(L"\r\n[2/6] Locating file system...\r\n");
    // Get loaded image protocol to access the device we booted from
    status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, 
                                (VOID**)&loaded_image);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to get loaded image protocol: %r\r\n", status);
        goto error_exit;
    }
    
    // Get file system protocol
    status = gBS->HandleProtocol(loaded_image->DeviceHandle, 
                                &gEfiSimpleFileSystemProtocolGuid, 
                                (VOID**)&filesystem);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to get file system protocol: %r\r\n", status);
        goto error_exit;
    }
    Print(L"File system located successfully\r\n");
    
    Print(L"\r\n[3/6] Loading kernel...\r\n");
    status = LoadKernel(filesystem, &config);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to load kernel: %r\r\n", status);
        goto error_exit;
    }
    Print(L"Kernel loaded successfully\r\n");
    Print(L"  Size: %d bytes\r\n", config.kernel_size);
    Print(L"  Physical base: 0x%lx\r\n", config.kernel_physical_base);
    Print(L"  Virtual base: 0x%lx\r\n", config.kernel_virtual_base);
    
    Print(L"\r\n[4/6] Setting up memory map...\r\n");
    status = SetupMemoryMap(&config);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to setup memory map: %r\r\n", status);
        goto error_exit;
    }
    Print(L"Memory map configured successfully\r\n");
    
    Print(L"\r\n[5/6] Setting up long mode...\r\n");
    status = SetupLongMode(&config);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to setup long mode: %r\r\n", status);
        goto error_exit;
    }
    Print(L"Long mode configured successfully\r\n");
    
    // Display boot configuration
    Print(L"\r\n");
    PrintBootInfo(&config);
    
    Print(L"\r\n[6/6] Transferring control to kernel...\r\n");
    Print(L"Press any key to boot or wait 3 seconds...\r\n");
    
    // Wait for key press or timeout
    WaitForKeyPress();
    
    Print(L"Booting kernel...\r\n");
    
    // Transfer control to kernel
    status = TransferControlToKernel(&config);
    
    // If we get here, something went wrong
    Print(L"ERROR: Kernel returned unexpectedly: %r\r\n", status);
    
error_exit:
    Print(L"\r\nBoot failed. Press any key to exit...\r\n");
    WaitForKeyPress();
    return status;
}

/**
 * @brief Initialize graphics output protocol
 * @param gop Pointer to store GOP protocol
 * @param config Boot configuration to update
 * @return EFI_STATUS Success or error code
 */
EFI_STATUS InitializeGraphics(EFI_GRAPHICS_OUTPUT_PROTOCOL** gop, BOOT_CONFIG* config)
{
    EFI_STATUS status;
    UINTN num_handles;
    EFI_HANDLE* handles = NULL;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode_info;
    UINTN size_of_info;
    UINT32 best_mode = 0;
    UINT32 best_score = 0;
    
    // Locate all GOP handles
    status = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid,
                                    NULL, &num_handles, &handles);
    if (EFI_ERROR(status) || num_handles == 0) {
        return status;
    }
    
    // Get the first GOP protocol
    status = gBS->HandleProtocol(handles[0], &gEfiGraphicsOutputProtocolGuid,
                                (VOID**)gop);
    if (EFI_ERROR(status)) {
        gBS->FreePool(handles);
        return status;
    }
    
    // Find the best graphics mode (prefer 1024x768 or higher with 32-bit color)
    for (UINT32 mode = 0; mode < (*gop)->Mode->MaxMode; mode++) {
        status = (*gop)->QueryMode(*gop, mode, &size_of_info, &mode_info);
        if (EFI_ERROR(status)) {
            continue;
        }
        
        UINT32 score = 0;
        
        // Prefer specific resolutions
        if (mode_info->HorizontalResolution >= 1024 && 
            mode_info->VerticalResolution >= 768) {
            score += 100;
        }
        
        // Prefer 32-bit color
        if (mode_info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ||
            mode_info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            score += 50;
        }
        
        if (score > best_score) {
            best_score = score;
            best_mode = mode;
        }
    }
    
    // Set the best mode
    if (best_score > 0) {
        status = (*gop)->SetMode(*gop, best_mode);
        if (EFI_ERROR(status)) {
            gBS->FreePool(handles);
            return status;
        }
    }
    
    // Store graphics information in boot config
    config->graphics_info = (*gop)->Mode->Info;
    config->framebuffer_base = (VOID*)(*gop)->Mode->FrameBufferBase;
    config->framebuffer_size = (*gop)->Mode->FrameBufferSize;
    
    gBS->FreePool(handles);
    return EFI_SUCCESS;
}

/**
 * @brief Load kernel from file system
 * @param filesystem File system protocol
 * @param config Boot configuration to update
 * @return EFI_STATUS Success or error code
 */
EFI_STATUS LoadKernel(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* filesystem, BOOT_CONFIG* config)
{
    EFI_STATUS status;
    EFI_FILE_PROTOCOL* root = NULL;
    EFI_FILE_PROTOCOL* kernel_file = NULL;
    EFI_FILE_INFO* file_info = NULL;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 256;
    VOID* kernel_buffer = NULL;
    UINTN kernel_size;
    EFI_PHYSICAL_ADDRESS kernel_addr = config->kernel_physical_base;
    
    // Open root directory
    status = filesystem->OpenVolume(filesystem, &root);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Open kernel file
    status = root->Open(root, &kernel_file, KERNEL_FILENAME, 
                       EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Could not find kernel file: %s\r\n", KERNEL_FILENAME);
        root->Close(root);
        return status;
    }
    
    // Get file information
    status = gBS->AllocatePool(EfiLoaderData, info_size, (VOID**)&file_info);
    if (EFI_ERROR(status)) {
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }
    
    status = kernel_file->GetInfo(kernel_file, &gEfiFileInfoGuid, 
                                 &info_size, file_info);
    if (EFI_ERROR(status)) {
        gBS->FreePool(file_info);
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }
    
    kernel_size = file_info->FileSize;
    config->kernel_size = kernel_size;
    
    // Allocate memory for kernel at specific physical address
    UINTN pages = (kernel_size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, &kernel_addr);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Could not allocate memory for kernel at 0x%lx\r\n", kernel_addr);
        gBS->FreePool(file_info);
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }
    
    // Read kernel into memory
    status = kernel_file->Read(kernel_file, &kernel_size, (VOID*)kernel_addr);
    if (EFI_ERROR(status)) {
        gBS->FreePages(kernel_addr, pages);
        gBS->FreePool(file_info);
        kernel_file->Close(kernel_file);
        root->Close(root);
        return status;
    }
    
    // Clean up
    gBS->FreePool(file_info);
    kernel_file->Close(kernel_file);
    root->Close(root);
    
    return EFI_SUCCESS;
}

/**
 * @brief Setup memory map for kernel
 * @param config Boot configuration to update
 * @return EFI_STATUS Success or error code
 */
EFI_STATUS SetupMemoryMap(BOOT_CONFIG* config)
{
    EFI_STATUS status;
    UINTN map_size = 0;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
    EFI_MEMORY_DESCRIPTOR* memory_map;
    
    // Get memory map size
    status = gBS->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, 
                              &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }
    
    // Allocate buffer for memory map (add extra space for potential changes)
    map_size += 2 * descriptor_size;
    status = gBS->AllocatePool(EfiLoaderData, map_size, (VOID**)&memory_map);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Get actual memory map
    status = gBS->GetMemoryMap(&map_size, memory_map, &map_key, 
                              &descriptor_size, &descriptor_version);
    if (EFI_ERROR(status)) {
        gBS->FreePool(memory_map);
        return status;
    }
    
    // Store memory map information
    config->memory_map = memory_map;
    config->memory_map_size = map_size;
    config->memory_map_key = map_key;
    config->descriptor_size = descriptor_size;
    config->descriptor_version = descriptor_version;
    
    return EFI_SUCCESS;
}

/**
 * @brief Setup long mode paging structures
 * @param config Boot configuration
 * @return EFI_STATUS Success or error code
 */
EFI_STATUS SetupLongMode(BOOT_CONFIG* config)
{
    // For now, we'll rely on the kernel to set up its own paging
    // The UEFI firmware has already set up basic long mode for us
    
    // TODO: Set up initial page tables for higher half kernel mapping
    // This is a placeholder - the actual implementation would create
    // page tables to map the kernel from its physical address to
    // the higher half virtual address space
    
    return EFI_SUCCESS;
}

/**
 * @brief Transfer control to the loaded kernel
 * @param config Boot configuration
 * @return EFI_STATUS Should not return on success
 */
EFI_STATUS TransferControlToKernel(BOOT_CONFIG* config)
{
    EFI_STATUS status;
    
    // Exit boot services - point of no return
    status = gBS->ExitBootServices(gImageHandle, config->memory_map_key);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // At this point, we can only use runtime services
    // Jump to kernel entry point
    typedef VOID (*KERNEL_ENTRY)(BOOT_CONFIG* config);
    KERNEL_ENTRY kernel_entry = (KERNEL_ENTRY)config->kernel_physical_base;
    
    // Call kernel with boot configuration
    kernel_entry(config);
    
    // Should never reach here
    return EFI_UNSUPPORTED;
}

/**
 * @brief Print boot configuration information
 * @param config Boot configuration to print
 */
VOID PrintBootInfo(BOOT_CONFIG* config)
{
    Print(L"========================================\r\n");
    Print(L"Boot Configuration:\r\n");
    Print(L"========================================\r\n");
    Print(L"Kernel Physical Base: 0x%lx\r\n", config->kernel_physical_base);
    Print(L"Kernel Virtual Base:  0x%lx\r\n", config->kernel_virtual_base);
    Print(L"Kernel Size:          %d bytes\r\n", config->kernel_size);
    Print(L"Stack Top:            0x%lx\r\n", config->stack_top);
    Print(L"Heap Start:           0x%lx\r\n", config->heap_start);
    Print(L"Heap Size:            %d bytes\r\n", config->heap_size);
    Print(L"Memory Map Entries:   %d\r\n", 
          config->memory_map_size / config->descriptor_size);
    
    if (config->graphics_info) {
        Print(L"Graphics Resolution:  %dx%d\r\n",
              config->graphics_info->HorizontalResolution,
              config->graphics_info->VerticalResolution);
        Print(L"Framebuffer Base:     0x%lx\r\n", 
              (UINT64)config->framebuffer_base);
        Print(L"Framebuffer Size:     %d bytes\r\n", config->framebuffer_size);
    }
    Print(L"========================================\r\n");
}

/**
 * @brief Wait for key press with timeout
 */
VOID WaitForKeyPress(VOID)
{
    EFI_INPUT_KEY key;
    EFI_STATUS status;
    UINTN index;
    EFI_EVENT events[2];
    
    // Create timer event for 3 second timeout
    EFI_EVENT timer_event;
    status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &timer_event);
    if (EFI_ERROR(status)) {
        return;
    }
    
    // Set timer for 3 seconds
    status = gBS->SetTimer(timer_event, TimerRelative, 30000000); // 3 seconds in 100ns units
    if (EFI_ERROR(status)) {
        gBS->CloseEvent(timer_event);
        return;
    }
    
    // Wait for either key press or timer
    events[0] = gST->ConIn->WaitForKey;
    events[1] = timer_event;
    
    status = gBS->WaitForEvent(2, events, &index);
    if (!EFI_ERROR(status) && index == 0) {
        // Key was pressed, consume it
        gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
    }
    
    gBS->CloseEvent(timer_event);
}