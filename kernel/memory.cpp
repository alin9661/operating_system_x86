/**
 * @file memory.cpp
 * @brief Modern 64-bit memory management with buddy allocator
 * 
 * This file implements a sophisticated memory management system for x86_64
 * architecture with UEFI integration, featuring a buddy allocator for 
 * efficient page allocation and fragmentation reduction.
 */

#include "../include/memory.h"
#include <algorithm>
#include <cassert>
#include <bit>
#include <new>

namespace Memory {
    
    // Constants for buddy allocator
    constexpr std::size_t MAX_BUDDY_ORDER = 10; // 4KB to 4MB blocks
    constexpr std::size_t MIN_BUDDY_ORDER = 0;  // Minimum 4KB pages
    constexpr std::size_t BUDDY_ORDERS = MAX_BUDDY_ORDER + 1;
    
    // Forward declarations
    class PageAllocator;
    class BuddyAllocator;
    
    // Global state
    static MemoryStatistics g_memory_stats;
    static BuddyAllocator* g_buddy_allocator = nullptr;
    static bool g_memory_initialized = false;
    static std::span<const MemoryMapEntry> g_memory_map;
    
    // Simple intrusive linked list for free blocks
    struct FreeBlock {
        FreeBlock* next;
        FreeBlock* prev;
        std::size_t order;
        
        FreeBlock() noexcept : next(nullptr), prev(nullptr), order(0) {}
    };
    
    // Free list for each buddy order
    struct FreeList {
        FreeBlock* head;
        std::atomic<std::size_t> count;
        
        FreeList() noexcept : head(nullptr), count(0) {}
        
        void push_front(FreeBlock* block) noexcept {
            block->next = head;
            block->prev = nullptr;
            if (head) {
                head->prev = block;
            }
            head = block;
            count.fetch_add(1, std::memory_order_relaxed);
        }
        
        FreeBlock* pop_front() noexcept {
            if (!head) return nullptr;
            
            FreeBlock* block = head;
            head = head->next;
            if (head) {
                head->prev = nullptr;
            }
            count.fetch_sub(1, std::memory_order_relaxed);
            return block;
        }
        
        void remove(FreeBlock* block) noexcept {
            if (block->prev) {
                block->prev->next = block->next;
            } else {
                head = block->next;
            }
            
            if (block->next) {
                block->next->prev = block->prev;
            }
            
            count.fetch_sub(1, std::memory_order_relaxed);
        }
        
        [[nodiscard]] bool empty() const noexcept {
            return head == nullptr;
        }
    };
    
    // Buddy allocator implementation
    class BuddyAllocator {
    private:
        std::array<FreeList, BUDDY_ORDERS> free_lists_;
        PhysicalAddress memory_start_;
        PhysicalAddress memory_end_;
        std::size_t total_pages_;
        
        // Metadata for tracking block allocation status
        // We use a simple bitmap where each bit represents allocation status
        static constexpr std::size_t BITS_PER_BYTE = 8;
        std::uint8_t* allocation_bitmap_;
        std::size_t bitmap_size_;
        
        // Calculate buddy address for a given address and order
        [[nodiscard]] PhysicalAddress get_buddy_address(PhysicalAddress addr, std::size_t order) const noexcept {
            const std::size_t block_size = PAGE_SIZE << order;
            return addr ^ block_size;
        }
        
        // Check if a block is allocated
        [[nodiscard]] bool is_allocated(PhysicalAddress addr, std::size_t /* order */) const noexcept {
            const std::size_t page_index = (addr - memory_start_) / PAGE_SIZE;
            const std::size_t bitmap_index = page_index / BITS_PER_BYTE;
            const std::size_t bit_offset = page_index % BITS_PER_BYTE;
            
            if (bitmap_index >= bitmap_size_) return true; // Out of range = allocated
            
            return allocation_bitmap_[bitmap_index] & (1 << bit_offset);
        }
        
        // Mark block as allocated or free
        void set_allocated(PhysicalAddress addr, std::size_t order, bool allocated) noexcept {
            const std::size_t pages_in_block = 1ULL << order;
            const std::size_t start_page = (addr - memory_start_) / PAGE_SIZE;
            
            for (std::size_t i = 0; i < pages_in_block; ++i) {
                const std::size_t page_index = start_page + i;
                const std::size_t bitmap_index = page_index / BITS_PER_BYTE;
                const std::size_t bit_offset = page_index % BITS_PER_BYTE;
                
                if (bitmap_index < bitmap_size_) {
                    if (allocated) {
                        allocation_bitmap_[bitmap_index] |= (1 << bit_offset);
                    } else {
                        allocation_bitmap_[bitmap_index] &= ~(1 << bit_offset);
                    }
                }
            }
        }
        
        // Split a block into two smaller blocks
        void split_block(FreeBlock* block, std::size_t target_order) noexcept {
            while (block->order > target_order) {
                const std::size_t new_order = block->order - 1;
                const std::size_t block_size = PAGE_SIZE << new_order;
                
                // Create buddy block
                const PhysicalAddress block_addr = reinterpret_cast<PhysicalAddress>(block);
                const PhysicalAddress buddy_addr = block_addr + block_size;
                
                FreeBlock* buddy = reinterpret_cast<FreeBlock*>(physical_to_virtual(buddy_addr));
                buddy->order = new_order;
                buddy->next = nullptr;
                buddy->prev = nullptr;
                
                // Update original block
                block->order = new_order;
                
                // Add buddy to appropriate free list
                free_lists_[new_order].push_front(buddy);
            }
        }
        
        // Try to coalesce block with its buddy
        [[nodiscard]] FreeBlock* try_coalesce(FreeBlock* block) noexcept {
            while (block->order < MAX_BUDDY_ORDER) {
                const PhysicalAddress block_addr = virtual_to_physical_direct(reinterpret_cast<VirtualAddress>(block));
                const PhysicalAddress buddy_addr = get_buddy_address(block_addr, block->order);
                
                // Check if buddy is within our memory range
                if (buddy_addr < memory_start_ || buddy_addr >= memory_end_) {
                    break;
                }
                
                // Check if buddy is free and same order
                if (is_allocated(buddy_addr, block->order)) {
                    break;
                }
                
                FreeBlock* buddy = reinterpret_cast<FreeBlock*>(physical_to_virtual(buddy_addr));
                if (buddy->order != block->order) {
                    break;
                }
                
                // Remove buddy from free list
                free_lists_[buddy->order].remove(buddy);
                
                // Coalesce blocks (keep the lower address one)
                if (buddy_addr < block_addr) {
                    buddy->order++;
                    block = buddy;
                } else {
                    block->order++;
                }
            }
            
            return block;
        }
        
    public:
        explicit BuddyAllocator(PhysicalAddress start, PhysicalAddress end) noexcept 
            : memory_start_(start), memory_end_(end) {
            
            // Calculate total pages and bitmap size
            total_pages_ = (memory_end_ - memory_start_) / PAGE_SIZE;
            bitmap_size_ = (total_pages_ + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
            
            // Allocate bitmap (simple allocation for now)
            allocation_bitmap_ = reinterpret_cast<std::uint8_t*>(new std::uint8_t[bitmap_size_]);
            std::fill_n(allocation_bitmap_, bitmap_size_, 0xFF); // Mark all as allocated initially
            
            // Initialize free lists
            for (auto& list : free_lists_) {
                new (&list) FreeList();
            }
            
            // Add free memory regions to buddy allocator
            initialize_free_regions();
            
            // Update statistics
            g_memory_stats.total_pages.store(total_pages_);
            update_free_page_count();
        }
        
        void initialize_free_regions() noexcept {
            // Process memory map to find available regions
            for (const auto& entry : g_memory_map) {
                if (!entry.is_available()) continue;
                
                PhysicalAddress region_start = std::max(entry.base_addr, memory_start_);
                PhysicalAddress region_end = std::min(entry.end_addr(), memory_end_);
                
                if (region_start >= region_end) continue;
                
                // Align region to page boundaries
                region_start = page_align_up(region_start);
                region_end = page_align_down(region_end);
                
                if (region_start >= region_end) continue;
                
                // Add region to buddy allocator
                add_free_region(region_start, region_end - region_start);
            }
        }
        
        void add_free_region(PhysicalAddress start, std::size_t size) noexcept {
            if (size < PAGE_SIZE) return;
            
            PhysicalAddress current = start;
            std::size_t remaining = size;
            
            while (remaining >= PAGE_SIZE) {
                // Find largest power-of-2 block that fits
                const std::size_t pages_remaining = remaining / PAGE_SIZE;
                const std::size_t max_order = std::min(
                    static_cast<std::size_t>(std::bit_width(pages_remaining) - 1),
                    MAX_BUDDY_ORDER
                );
                
                // Check alignment for this order
                std::size_t order = max_order;
                while (order > 0 && (current & ((PAGE_SIZE << order) - 1)) != 0) {
                    --order;
                }
                
                const std::size_t block_size = PAGE_SIZE << order;
                
                // Create free block
                FreeBlock* block = reinterpret_cast<FreeBlock*>(physical_to_virtual(current));
                block->order = order;
                block->next = nullptr;
                block->prev = nullptr;
                
                // Mark as free in bitmap
                set_allocated(current, order, false);
                
                // Add to appropriate free list
                free_lists_[order].push_front(block);
                
                // Move to next block
                current += block_size;
                remaining -= block_size;
            }
        }
        
        void update_free_page_count() noexcept {
            std::size_t total_free = 0;
            for (std::size_t order = 0; order <= MAX_BUDDY_ORDER; ++order) {
                const std::size_t pages_per_block = 1ULL << order;
                total_free += free_lists_[order].count.load() * pages_per_block;
            }
            
            g_memory_stats.free_pages.store(total_free);
            g_memory_stats.used_pages.store(total_pages_ - total_free);
        }
        
        [[nodiscard]] AllocResult<PhysicalAddress> allocate(std::size_t pages) noexcept {
            if (pages == 0) {
                return compat::unexpected(MemoryError::InvalidSize);
            }
            
            // Find the order needed for this allocation
            const std::size_t order = std::bit_width(pages - 1); // Ceiling log2
            if (order > MAX_BUDDY_ORDER) {
                return compat::unexpected(MemoryError::InvalidSize);
            }
            
            // Find a block of the appropriate size
            for (std::size_t search_order = order; search_order <= MAX_BUDDY_ORDER; ++search_order) {
                if (free_lists_[search_order].empty()) continue;
                
                // Found a block, remove it from free list
                FreeBlock* block = free_lists_[search_order].pop_front();
                
                // Split if necessary
                if (block->order > order) {
                    split_block(block, order);
                }
                
                // Mark as allocated
                const PhysicalAddress addr = virtual_to_physical_direct(reinterpret_cast<VirtualAddress>(block));
                set_allocated(addr, order, true);
                
                // Update statistics
                update_free_page_count();
                
                return addr;
            }
            
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        [[nodiscard]] AllocResult<void> deallocate(PhysicalAddress addr, std::size_t pages) noexcept {
            if (pages == 0) {
                return compat::unexpected(MemoryError::InvalidSize);
            }
            
            if (!is_page_aligned(addr) || addr < memory_start_ || addr >= memory_end_) {
                return compat::unexpected(MemoryError::InvalidAddress);
            }
            
            const std::size_t order = std::bit_width(pages - 1); // Ceiling log2
            if (order > MAX_BUDDY_ORDER) {
                return compat::unexpected(MemoryError::InvalidSize);
            }
            
            // Mark as free in bitmap
            set_allocated(addr, order, false);
            
            // Create free block
            FreeBlock* block = reinterpret_cast<FreeBlock*>(physical_to_virtual(addr));
            block->order = order;
            block->next = nullptr;
            block->prev = nullptr;
            
            // Try to coalesce with buddies
            block = try_coalesce(block);
            
            // Add to appropriate free list
            free_lists_[block->order].push_front(block);
            
            // Update statistics
            update_free_page_count();
            
            return {};
        }
        
        [[nodiscard]] const MemoryStatistics& get_statistics() const noexcept {
            return g_memory_stats;
        }
    };
    
    // Page table entry for 64-bit x86
    struct PageTableEntry {
        std::uint64_t value;
        
        [[nodiscard]] bool is_present() const noexcept { return value & 1; }
        [[nodiscard]] bool is_writable() const noexcept { return value & 2; }
        [[nodiscard]] bool is_user() const noexcept { return value & 4; }
        [[nodiscard]] PhysicalAddress get_address() const noexcept { return value & 0x000FFFFFFFFFF000ULL; }
        
        void set_address(PhysicalAddress addr) noexcept {
            value = (value & 0xFFF0000000000FFFULL) | (addr & 0x000FFFFFFFFFF000ULL);
        }
        
        void set_flags(PageFlags flags) noexcept {
            value = (value & 0x000FFFFFFFFFF000ULL) | static_cast<std::uint64_t>(flags);
        }
    };
    
    // 64-bit page table structure
    struct PageTable {
        std::array<PageTableEntry, 512> entries;
    };
    
    // Global page table for kernel mapping
    static PageTable* g_kernel_pml4 = nullptr;
    
    // Implementation of public interface functions
    
    [[nodiscard]] AllocResult<void> init(std::span<const MemoryMapEntry> memory_map) noexcept {
        if (g_memory_initialized) {
            return {}; // Already initialized
        }
        
        g_memory_map = memory_map;
        
        // Find the largest conventional memory region for our allocator
        PhysicalAddress best_start = 0;
        PhysicalAddress best_end = 0;
        std::size_t best_size = 0;
        
        for (const auto& entry : memory_map) {
            if (!entry.is_available()) continue;
            
            if (entry.length > best_size) {
                best_start = page_align_up(entry.base_addr);
                best_end = page_align_down(entry.end_addr());
                best_size = best_end - best_start;
            }
        }
        
        if (best_size < PAGE_SIZE) {
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        // Reserve space for allocator itself (simple placement)
        const std::size_t allocator_size = sizeof(BuddyAllocator);
        if (best_size < allocator_size + PAGE_SIZE) {
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        // Create buddy allocator
        const PhysicalAddress allocator_start = best_start + ((allocator_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
        g_buddy_allocator = new(reinterpret_cast<void*>(physical_to_virtual(best_start))) 
                                BuddyAllocator(allocator_start, best_end);
        
        g_memory_initialized = true;
        return {};
    }
    
    [[nodiscard]] AllocResult<PhysicalAddress> allocate_page(PageFlags /* flags */) noexcept {
        if (!g_memory_initialized || !g_buddy_allocator) {
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        return g_buddy_allocator->allocate(1);
    }
    
    [[nodiscard]] AllocResult<PhysicalAddress> allocate_pages(std::size_t count, PageFlags /* flags */) noexcept {
        if (!g_memory_initialized || !g_buddy_allocator) {
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        if (count == 0) {
            return compat::unexpected(MemoryError::InvalidSize);
        }
        
        return g_buddy_allocator->allocate(count);
    }
    
    [[nodiscard]] AllocResult<void> free_page(PhysicalAddress addr) noexcept {
        if (!g_memory_initialized || !g_buddy_allocator) {
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        return g_buddy_allocator->deallocate(addr, 1);
    }
    
    [[nodiscard]] AllocResult<void> free_pages(PhysicalAddress addr, std::size_t count) noexcept {
        if (!g_memory_initialized || !g_buddy_allocator) {
            return compat::unexpected(MemoryError::OutOfMemory);
        }
        
        return g_buddy_allocator->deallocate(addr, count);
    }
    
    [[nodiscard]] AllocResult<void> map_page(PhysicalAddress physical_addr, VirtualAddress virtual_addr, PageFlags flags) noexcept {
        // TODO: Implement page table mapping
        // For now, we assume direct mapping in higher half
        return {};
    }
    
    [[nodiscard]] AllocResult<void> unmap_page(VirtualAddress virtual_addr) noexcept {
        // TODO: Implement page table unmapping
        return {};
    }
    
    [[nodiscard]] AllocResult<PhysicalAddress> virtual_to_physical(VirtualAddress virtual_addr) noexcept {
        // For direct mapped higher half, simple calculation
        if (virtual_addr >= KERNEL_VIRTUAL_BASE) {
            return virtual_addr - KERNEL_VIRTUAL_BASE;
        }
        
        // TODO: Implement page table walk for other mappings
        return compat::unexpected(MemoryError::NotMapped);
    }
    
    [[nodiscard]] const MemoryStatistics& get_statistics() noexcept {
        if (g_buddy_allocator) {
            return g_buddy_allocator->get_statistics();
        }
        return g_memory_stats;
    }
    
    // Kernel allocator implementation
    template<typename T>
    typename KernelAllocator<T>::pointer KernelAllocator<T>::allocate(size_type n) {
        const std::size_t bytes = n * sizeof(T);
        const std::size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        
        auto result = allocate_pages(pages);
        if (!result.has_value()) {
            throw std::bad_alloc{};
        }
        
        return reinterpret_cast<pointer>(physical_to_virtual(result.value()));
    }
    
    template<typename T>
    void KernelAllocator<T>::deallocate(pointer p, size_type n) noexcept {
        if (!p) return;
        
        const std::size_t bytes = n * sizeof(T);
        const std::size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        const PhysicalAddress addr = virtual_to_physical_direct(reinterpret_cast<VirtualAddress>(p));
        
        free_pages(addr, pages);
    }
    
    // Explicit template instantiations for common types
    template class KernelAllocator<std::byte>;
    template class KernelAllocator<char>;
    template class KernelAllocator<int>;
    template class KernelAllocator<std::uint64_t>;
    
    // NoThrow memory operations
    namespace NoThrow {
        void* malloc(std::size_t size) noexcept {
            if (size == 0) return nullptr;
            
            const std::size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
            auto result = allocate_pages(pages);
            
            if (!result.has_value()) {
                return nullptr;
            }
            
            return reinterpret_cast<void*>(physical_to_virtual(result.value()));
        }
        
        void* calloc(std::size_t num, std::size_t size) noexcept {
            const std::size_t total_size = num * size;
            void* ptr = malloc(total_size);
            
            if (ptr) {
                std::fill_n(static_cast<std::byte*>(ptr), total_size, std::byte{0});
            }
            
            return ptr;
        }
        
        void* realloc(void* ptr, std::size_t size) noexcept {
            if (!ptr) return malloc(size);
            if (size == 0) {
                free(ptr);
                return nullptr;
            }
            
            // Simple implementation: allocate new, copy, free old
            void* new_ptr = malloc(size);
            if (!new_ptr) return nullptr;
            
            // TODO: Determine old size for proper copying
            // For now, assume worst case
            std::copy_n(static_cast<const std::byte*>(ptr), size, static_cast<std::byte*>(new_ptr));
            free(ptr);
            
            return new_ptr;
        }
        
        void free(void* ptr) noexcept {
            if (!ptr) return;
            
            const PhysicalAddress addr = virtual_to_physical_direct(reinterpret_cast<VirtualAddress>(ptr));
            // TODO: Determine actual size allocated
            // For now, assume single page
            free_page(addr);
        }
        
        void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept {
            // Buddy allocator already provides power-of-2 alignment
            return malloc(size);
        }
    }
    
    #ifdef DEBUG
    void dump_memory_map() noexcept {
        // TODO: Implement debug output
    }
    
    void dump_page_tables() noexcept {
        // TODO: Implement debug output
    }
    
    void validate_heap() noexcept {
        // TODO: Implement heap validation
    }
    
    bool check_memory_corruption() noexcept {
        // TODO: Implement corruption checking
        return false;
    }
    #endif
    
} // namespace Memory

// Global operator overloads for kernel memory
void* operator new(std::size_t size) noexcept {
    return Memory::NoThrow::malloc(size);
}

void* operator new[](std::size_t size) noexcept {
    return Memory::NoThrow::malloc(size);
}

void operator delete(void* ptr) noexcept {
    Memory::NoThrow::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    Memory::NoThrow::free(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept {
    (void)size; // Unused parameter
    Memory::NoThrow::free(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
    (void)size; // Unused parameter
    Memory::NoThrow::free(ptr);
}