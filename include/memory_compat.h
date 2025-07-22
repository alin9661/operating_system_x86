#pragma once

#include "compat/cstdint"
#include "compat/cstddef"
#include "compat/expected"

namespace Memory {
    // Constants
    static const std::size_t PAGE_SIZE = 4096;
    static const std::size_t PAGE_SHIFT = 12;
    static const std::size_t KERNEL_HEAP_SIZE = 16 * 1024 * 1024; // 16MB
    static const std::uint64_t KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;
    
    // Address types
    typedef std::uint64_t PhysicalAddress;
    typedef std::uint64_t VirtualAddress;
    
    // Memory error types
    enum class MemoryError {
        Success,
        OutOfMemory,
        InvalidAddress,
        InvalidSize,
        InvalidAlignment,
        PermissionDenied,
        AlreadyMapped,
        NotMapped,
        Fragmented,
        InitializationFailed
    };
    
    // Memory map entry type
    enum class MemoryType : std::uint32_t {
        EfiReservedMemoryType = 0,
        EfiLoaderCode = 1,
        EfiLoaderData = 2,
        EfiBootServicesCode = 3,
        EfiBootServicesData = 4,
        EfiRuntimeServicesCode = 5,
        EfiRuntimeServicesData = 6,
        EfiConventionalMemory = 7,
        EfiUnusableMemory = 8,
        EfiACPIReclaimMemory = 9,
        EfiACPIMemoryNVS = 10,
        EfiMemoryMappedIO = 11,
        EfiMemoryMappedIOPortSpace = 12,
        EfiPalCode = 13,
        EfiMaxMemoryType = 14
    };
    
    // Memory attributes
    enum class MemoryAttributes : std::uint64_t {
        UC = 0x0000000000000001,   // Uncached
        WC = 0x0000000000000002,   // Write Combining
        WT = 0x0000000000000004,   // Write Through
        WB = 0x0000000000000008,   // Write Back
        UCE = 0x0000000000000010,  // Uncached, exported
        WP = 0x0000000000001000,   // Write Protected
        RP = 0x0000000000002000,   // Read Protected
        XP = 0x0000000000004000,   // Execute Protected
        NV = 0x0000000000008000,   // Non-Volatile
        RO = 0x0000000000020000,   // Read-Only
    };
    
    // Memory map entry
    struct MemoryMapEntry {
        MemoryType type;
        PhysicalAddress base_addr;
        VirtualAddress virtual_start;
        std::uint64_t length;
        MemoryAttributes extended_attributes;
        
        PhysicalAddress end_addr() const {
            return base_addr + length;
        }
        
        bool is_available() const {
            return type == MemoryType::EfiConventionalMemory;
        }
    };
    
    // Page flags
    enum class PageFlags : std::uint64_t {
        None = 0,
        Present = 1 << 0,
        Writable = 1 << 1,
        UserAccessible = 1 << 2,
        WriteThrough = 1 << 3,
        CacheDisable = 1 << 4,
        Accessed = 1 << 5,
        Dirty = 1 << 6,
        HugePage = 1 << 7,
        Global = 1 << 8,
        NoExecute = 1ULL << 63
    };
    
    // Result type
    template<typename T>
    using AllocResult = std::expected<T, MemoryError>;
    
    // Basic utility functions
    bool is_page_aligned(std::uint64_t addr);
    std::uint64_t page_align_down(std::uint64_t addr);
    std::uint64_t page_align_up(std::uint64_t addr);
    
    // Address conversion
    PhysicalAddress virtual_to_physical_direct(VirtualAddress vaddr);
    VirtualAddress physical_to_virtual(PhysicalAddress paddr);
    
    // Core memory management functions
    AllocResult<void> init(const MemoryMapEntry* memory_map, std::size_t entry_count);
    AllocResult<PhysicalAddress> allocate_page(PageFlags flags = PageFlags::Present);
    AllocResult<PhysicalAddress> allocate_pages(std::size_t count, PageFlags flags = PageFlags::Present);
    AllocResult<void> free_page(PhysicalAddress addr);
    AllocResult<void> free_pages(PhysicalAddress addr, std::size_t count);
    AllocResult<void> map_page(PhysicalAddress physical_addr, VirtualAddress virtual_addr, PageFlags flags);
    AllocResult<void> unmap_page(VirtualAddress virtual_addr);
    
    // Memory statistics
    struct MemoryStatistics {
        std::size_t total_pages;
        std::size_t free_pages;
        std::size_t used_pages;
        std::size_t kernel_pages;
        std::size_t user_pages;
        std::size_t fragmentation_percentage;
    };
    
    const MemoryStatistics& get_memory_statistics();
}