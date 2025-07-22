#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <concepts>
#include <span>
#include <array>
#include <atomic>
#include <bit>

// Compatibility for std::expected (not available in all C++20 implementations)
#if __has_include(<expected>) && __cplusplus >= 202302L
#include <expected>
namespace compat {
    template<typename T, typename E>
    using expected = std::expected<T, E>;
    template<typename E>
    using unexpected = std::unexpected<E>;
}
#else
// Simple implementation for systems without std::expected
namespace compat {
    template<typename E>
    struct unexpected {
        E error;
        explicit unexpected(E e) : error(e) {}
    };
    
    template<typename T, typename E>
    class expected {
        union {
            T value_;
            E error_;
        };
        bool has_value_;
        
    public:
        expected(T value) : value_(value), has_value_(true) {}
        expected(unexpected<E> err) : error_(err.error), has_value_(false) {}
        
        ~expected() {
            if (has_value_) {
                value_.~T();
            } else {
                error_.~E();
            }
        }
        
        bool has_value() const noexcept { return has_value_; }
        operator bool() const noexcept { return has_value_; }
        
        T& value() & { return value_; }
        const T& value() const& { return value_; }
        T&& value() && { return std::move(value_); }
        
        E& error() & { return error_; }
        const E& error() const& { return error_; }
    };
    
    // Specialization for void
    template<typename E>
    class expected<void, E> {
        E error_;
        bool has_value_;
        
    public:
        expected() : has_value_(true) {}
        expected(unexpected<E> err) : error_(err.error), has_value_(false) {}
        
        bool has_value() const noexcept { return has_value_; }
        operator bool() const noexcept { return has_value_; }
        
        void value() const { /* void */ }
        
        E& error() & { return error_; }
        const E& error() const& { return error_; }
    };
}
#endif

namespace Memory {
    // Modern C++20 constants
    inline constexpr std::size_t PAGE_SIZE = 4096;
    inline constexpr std::size_t PAGE_SHIFT = 12;
    inline constexpr std::size_t KERNEL_HEAP_SIZE = 16 * 1024 * 1024; // 16MB
    inline constexpr std::uint64_t KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000ULL;
    
    // Memory allocation alignment
    inline constexpr std::size_t CACHE_LINE_SIZE = 64;
    inline constexpr std::size_t MAX_ALIGNMENT = alignof(std::max_align_t);
    
    // Error types for memory operations
    enum class MemoryError : std::uint8_t {
        OutOfMemory,
        InvalidAddress,
        InvalidSize,
        InvalidAlignment,
        AlreadyMapped,
        NotMapped,
        PermissionDenied,
        FragmentationError
    };
    
    // Memory map entry types (UEFI specification)
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
        EfiPersistentMemory = 14,
        EfiMaxMemoryType = 15
    };
    
    // Memory attributes
    enum class MemoryAttributes : std::uint64_t {
        UC = 0x0000000000000001ULL,  // Uncacheable
        WC = 0x0000000000000002ULL,  // Write Combining
        WT = 0x0000000000000004ULL,  // Write Through
        WB = 0x0000000000000008ULL,  // Write Back
        UCE = 0x0000000000000010ULL, // Uncacheable exported
        WP = 0x0000000000001000ULL,  // Write Protected
        RP = 0x0000000000002000ULL,  // Read Protected
        XP = 0x0000000000004000ULL,  // Execute Protected
        NV = 0x0000000000008000ULL,  // Non-Volatile
        MORE_RELIABLE = 0x0000000000010000ULL,
        RO = 0x0000000000020000ULL,  // Read Only
        SP = 0x0000000000040000ULL,  // Specific Purpose
        CPU_CRYPTO = 0x0000000000080000ULL,
        RUNTIME = 0x8000000000000000ULL
    };
    
    // Modern memory map entry with C++20 features
    struct MemoryMapEntry {
        std::uint64_t base_addr;
        std::uint64_t length;
        MemoryType type;
        MemoryAttributes extended_attributes;
        
        // C++20 defaulted comparison operators
        auto operator<=>(const MemoryMapEntry&) const = default;
        
        // Utility functions
        [[nodiscard]] constexpr std::uint64_t end_addr() const noexcept {
            return base_addr + length;
        }
        
        [[nodiscard]] constexpr bool is_available() const noexcept {
            return type == MemoryType::EfiConventionalMemory;
        }
        
        [[nodiscard]] constexpr bool contains(std::uint64_t addr) const noexcept {
            return addr >= base_addr && addr < end_addr();
        }
    };
    
    // Page frame number type
    using PageFrameNumber = std::uint64_t;
    
    // Virtual and physical address types
    using VirtualAddress = std::uint64_t;
    using PhysicalAddress = std::uint64_t;
    
    // Concepts for address validation
    template<typename T>
    concept AddressType = std::same_as<T, VirtualAddress> || std::same_as<T, PhysicalAddress>;
    
    template<AddressType T>
    [[nodiscard]] constexpr bool is_page_aligned(T addr) noexcept {
        return (addr & (PAGE_SIZE - 1)) == 0;
    }
    
    template<AddressType T>
    [[nodiscard]] constexpr T page_align_down(T addr) noexcept {
        return addr & ~(PAGE_SIZE - 1);
    }
    
    template<AddressType T>
    [[nodiscard]] constexpr T page_align_up(T addr) noexcept {
        return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
    
    // Page flags for memory management
    enum class PageFlags : std::uint64_t {
        Present = 1ULL << 0,
        Writable = 1ULL << 1,
        User = 1ULL << 2,
        WriteThrough = 1ULL << 3,
        CacheDisabled = 1ULL << 4,
        Accessed = 1ULL << 5,
        Dirty = 1ULL << 6,
        HugePage = 1ULL << 7,
        Global = 1ULL << 8,
        NoExecute = 1ULL << 63
    };
    
    // Enable bitwise operations on PageFlags
    constexpr PageFlags operator|(PageFlags a, PageFlags b) noexcept {
        return static_cast<PageFlags>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
    }
    
    constexpr PageFlags operator&(PageFlags a, PageFlags b) noexcept {
        return static_cast<PageFlags>(static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b));
    }
    
    constexpr PageFlags operator~(PageFlags a) noexcept {
        return static_cast<PageFlags>(~static_cast<std::uint64_t>(a));
    }
    
    // Memory statistics
    struct MemoryStatistics {
        std::atomic<std::size_t> total_pages{0};
        std::atomic<std::size_t> free_pages{0};
        std::atomic<std::size_t> used_pages{0};
        std::atomic<std::size_t> cached_pages{0};
        std::atomic<std::size_t> dirty_pages{0};
        std::atomic<std::size_t> active_pages{0};
        std::atomic<std::size_t> inactive_pages{0};
        
        [[nodiscard]] std::size_t total_memory() const noexcept {
            return total_pages.load() * PAGE_SIZE;
        }
        
        [[nodiscard]] std::size_t free_memory() const noexcept {
            return free_pages.load() * PAGE_SIZE;
        }
        
        [[nodiscard]] std::size_t used_memory() const noexcept {
            return used_pages.load() * PAGE_SIZE;
        }
        
        [[nodiscard]] double utilization() const noexcept {
            const auto total = total_pages.load();
            const auto used = used_pages.load();
            return total > 0 ? static_cast<double>(used) / total : 0.0;
        }
    };
    
    // Forward declarations
    class PageAllocator;
    class BuddyAllocator;
    class SlabAllocator;
    class VirtualMemoryManager;
    
    // Smart pointer types for kernel memory
    template<typename T>
    using KernelUniquePtr = std::unique_ptr<T, void(*)(T*)>;
    
    template<typename T>
    using KernelSharedPtr = std::shared_ptr<T>;
    
    // Memory allocation result type
    template<typename T>
    using AllocResult = compat::expected<T, MemoryError>;
    
    // Function declarations with modern C++20 features
    
    /// Initialize memory management subsystem
    /// @param memory_map Span of memory map entries from UEFI
    /// @return Success or error code
    [[nodiscard]] AllocResult<void> init(std::span<const MemoryMapEntry> memory_map) noexcept;
    
    /// Allocate a single page of memory
    /// @param flags Page flags for the allocation
    /// @return Physical address of allocated page or error
    [[nodiscard]] AllocResult<PhysicalAddress> allocate_page(PageFlags flags = PageFlags::Present | PageFlags::Writable) noexcept;
    
    /// Allocate multiple contiguous pages
    /// @param count Number of pages to allocate
    /// @param flags Page flags for the allocation
    /// @return Physical address of first page or error
    [[nodiscard]] AllocResult<PhysicalAddress> allocate_pages(std::size_t count, PageFlags flags = PageFlags::Present | PageFlags::Writable) noexcept;
    
    /// Free a single page of memory
    /// @param addr Physical address of page to free
    /// @return Success or error code
    [[nodiscard]] AllocResult<void> free_page(PhysicalAddress addr) noexcept;
    
    /// Free multiple contiguous pages
    /// @param addr Physical address of first page to free
    /// @param count Number of pages to free
    /// @return Success or error code
    [[nodiscard]] AllocResult<void> free_pages(PhysicalAddress addr, std::size_t count) noexcept;
    
    /// Map a physical address to a virtual address
    /// @param physical_addr Physical address to map
    /// @param virtual_addr Virtual address to map to
    /// @param flags Page flags for the mapping
    /// @return Success or error code
    [[nodiscard]] AllocResult<void> map_page(PhysicalAddress physical_addr, VirtualAddress virtual_addr, PageFlags flags) noexcept;
    
    /// Unmap a virtual address
    /// @param virtual_addr Virtual address to unmap
    /// @return Success or error code
    [[nodiscard]] AllocResult<void> unmap_page(VirtualAddress virtual_addr) noexcept;
    
    /// Translate virtual address to physical address
    /// @param virtual_addr Virtual address to translate
    /// @return Physical address or error
    [[nodiscard]] AllocResult<PhysicalAddress> virtual_to_physical(VirtualAddress virtual_addr) noexcept;
    
    /// Get memory statistics
    /// @return Current memory statistics
    [[nodiscard]] const MemoryStatistics& get_statistics() noexcept;
    
    /// Check if address is canonical for x86_64
    /// @param addr Virtual address to check
    /// @return true if address is canonical
    [[nodiscard]] constexpr bool is_canonical(VirtualAddress addr) noexcept {
        return (addr <= 0x00007FFFFFFFFFFF) || (addr >= 0xFFFF800000000000);
    }
    
    /// Convert physical address to higher half virtual address
    /// @param physical_addr Physical address to convert
    /// @return Virtual address in higher half
    [[nodiscard]] constexpr VirtualAddress physical_to_virtual(PhysicalAddress physical_addr) noexcept {
        return physical_addr + KERNEL_VIRTUAL_BASE;
    }
    
    /// Convert higher half virtual address to physical address
    /// @param virtual_addr Virtual address to convert
    /// @return Physical address
    [[nodiscard]] constexpr PhysicalAddress virtual_to_physical_direct(VirtualAddress virtual_addr) noexcept {
        return virtual_addr - KERNEL_VIRTUAL_BASE;
    }
    
    /// Allocate kernel memory with automatic cleanup
    /// @param size Size to allocate
    /// @param alignment Alignment requirement
    /// @return Unique pointer with custom deleter
    template<typename T = std::byte>
    [[nodiscard]] KernelUniquePtr<T> allocate_kernel_memory(std::size_t size, std::size_t alignment = alignof(T)) noexcept;
    
    /// Create kernel object with automatic cleanup
    /// @param args Constructor arguments
    /// @return Unique pointer to constructed object
    template<typename T, typename... Args>
    [[nodiscard]] KernelUniquePtr<T> make_kernel_unique(Args&&... args) noexcept;
    
    /// Kernel memory allocator for STL containers
    template<typename T>
    class KernelAllocator {
    public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        
        template<typename U>
        struct rebind {
            using other = KernelAllocator<U>;
        };
        
        KernelAllocator() noexcept = default;
        template<typename U>
        KernelAllocator(const KernelAllocator<U>&) noexcept {}
        
        [[nodiscard]] pointer allocate(size_type n);
        void deallocate(pointer p, size_type n) noexcept;
        
        template<typename U>
        bool operator==(const KernelAllocator<U>&) const noexcept { return true; }
        template<typename U>
        bool operator!=(const KernelAllocator<U>&) const noexcept { return false; }
    };
    
    // Memory pool for fast allocation of fixed-size objects
    template<typename T, std::size_t PoolSize = 1024>
    class MemoryPool {
    private:
        alignas(T) std::array<std::byte, sizeof(T) * PoolSize> storage_;
        std::array<bool, PoolSize> used_;
        std::atomic<std::size_t> next_free_{0};
        
    public:
        [[nodiscard]] T* allocate() noexcept;
        void deallocate(T* ptr) noexcept;
        [[nodiscard]] bool is_from_pool(const T* ptr) const noexcept;
        [[nodiscard]] std::size_t available() const noexcept;
    };
    
    // Exception-free memory operations
    namespace NoThrow {
        void* malloc(std::size_t size) noexcept;
        void* calloc(std::size_t num, std::size_t size) noexcept;
        void* realloc(void* ptr, std::size_t size) noexcept;
        void free(void* ptr) noexcept;
        void* aligned_alloc(std::size_t alignment, std::size_t size) noexcept;
    }
    
    // Debug and introspection functions
    #ifdef DEBUG
    void dump_memory_map() noexcept;
    void dump_page_tables() noexcept;
    void validate_heap() noexcept;
    bool check_memory_corruption() noexcept;
    #endif
    
} // namespace Memory

// Note: Global operators are implemented in memory.cpp to avoid conflicts 