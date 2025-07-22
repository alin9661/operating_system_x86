#pragma once
#include <stdint.h>
#include <stddef.h>

namespace Memory {
    // Constants
    const size_t PAGE_SIZE = 4096;
    
    // Simple memory management interface
    void init();
    void* allocate_page();
    void free_page(void* ptr);
    void map_page(uint32_t physical_addr, uint32_t virtual_addr);
    size_t get_total_memory();
    size_t get_free_memory();
}