#include "memory_compat.h"

namespace Memory {
    // Simple page allocation system
    static size_t total_pages = 1024; // 4MB
    static MemoryStatistics stats = {0, 0, 0, 0, 0, 0};
    
    bool is_page_aligned(std::uint64_t addr) {
        return (addr & (PAGE_SIZE - 1)) == 0;
    }
    
    std::uint64_t page_align_down(std::uint64_t addr) {
        return addr & ~(PAGE_SIZE - 1);
    }
    
    std::uint64_t page_align_up(std::uint64_t addr) {
        return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    PhysicalAddress virtual_to_physical_direct(VirtualAddress vaddr) {
        return vaddr; // Identity mapping for now
    }
    
    VirtualAddress physical_to_virtual(PhysicalAddress paddr) {
        return paddr; // Identity mapping for now
    }
    
    AllocResult<void> init(const MemoryMapEntry* /* memory_map */, std::size_t /* entry_count */) {
        // Simple initialization - just mark some pages as available
        stats.total_pages = total_pages;
        stats.free_pages = total_pages - 100; // Reserve some pages
        stats.used_pages = 100;
        return std::expected<void, MemoryError>();
    }
    
    AllocResult<PhysicalAddress> allocate_page(PageFlags /* flags */) {
        // Simple allocation - return a fixed address for now
        static PhysicalAddress next_addr = 0x200000; // Start at 2MB
        PhysicalAddress addr = next_addr;
        next_addr += PAGE_SIZE;
        
        if (stats.free_pages > 0) {
            stats.free_pages--;
            stats.used_pages++;
        }
        
        return addr;
    }
    
    AllocResult<PhysicalAddress> allocate_pages(std::size_t count, PageFlags flags) {
        if (count == 0) {
            return std::unexpected<MemoryError>(MemoryError::InvalidSize);
        }
        // Simple implementation - just allocate first page
        return allocate_page(flags);
    }
    
    AllocResult<void> free_page(PhysicalAddress /* addr */) {
        if (stats.used_pages > 0) {
            stats.used_pages--;
            stats.free_pages++;
        }
        return std::expected<void, MemoryError>();
    }
    
    AllocResult<void> free_pages(PhysicalAddress addr, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            auto result = free_page(addr + i * PAGE_SIZE);
            if (!result.has_value()) {
                return result;
            }
        }
        return std::expected<void, MemoryError>();
    }
    
    AllocResult<void> map_page(PhysicalAddress /* physical_addr */, VirtualAddress /* virtual_addr */, PageFlags /* flags */) {
        // Simple implementation - assume identity mapping
        return std::expected<void, MemoryError>();
    }
    
    AllocResult<void> unmap_page(VirtualAddress /* virtual_addr */) {
        return std::expected<void, MemoryError>();
    }
    
    const MemoryStatistics& get_memory_statistics() {
        return stats;
    }
}