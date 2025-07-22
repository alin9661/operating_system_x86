#include "memory.h"
#include <stdint.h>
#include <stddef.h>

namespace Memory {
    // Page directory entry
    struct PageDirectoryEntry {
        uint32_t present : 1;
        uint32_t rw : 1;
        uint32_t user : 1;
        uint32_t accessed : 1;
        uint32_t dirty : 1;
        uint32_t unused : 7;
        uint32_t frame : 20;
    };

    // Page table entry
    struct PageTableEntry {
        uint32_t present : 1;
        uint32_t rw : 1;
        uint32_t user : 1;
        uint32_t accessed : 1;
        uint32_t dirty : 1;
        uint32_t unused : 7;
        uint32_t frame : 20;
    };

    // Page table
    struct PageTable {
        PageTableEntry entries[1024];
    };

    // Page directory
    struct PageDirectory {
        PageDirectoryEntry entries[1024];
    };

    // Global page directory
    static PageDirectory* kernel_directory = nullptr;
    static PageDirectory* current_directory = nullptr;

    // Memory bitmap
    static uint32_t* memory_bitmap = nullptr;
    static size_t total_pages = 0;

    // Forward declaration
    static void switch_page_directory(PageDirectory* dir);

    // Initialize memory management
    void init() {
        // Allocate space for page directory
        kernel_directory = (PageDirectory*)allocate_page();
        
        // Clear the directory
        for (int i = 0; i < 1024; i++) {
            kernel_directory->entries[i].present = 0;
        }

        // Set up identity mapping for first 4MB
        for (uint32_t i = 0; i < 1024; i++) {
            PageTable* table = (PageTable*)allocate_page();
            
            for (uint32_t j = 0; j < 1024; j++) {
                table->entries[j].present = 1;
                table->entries[j].rw = 1;
                table->entries[j].user = 0;
                table->entries[j].frame = i * 1024 + j;
            }

            kernel_directory->entries[i].present = 1;
            kernel_directory->entries[i].rw = 1;
            kernel_directory->entries[i].user = 0;
            kernel_directory->entries[i].frame = (uint32_t)table >> 12;
        }

        // Switch to the new page directory
        current_directory = kernel_directory;
        switch_page_directory(current_directory);
    }

    // Allocate a page of memory
    void* allocate_page() {
        // Find first free page in bitmap
        for (size_t i = 0; i < total_pages; i++) {
            if (!(memory_bitmap[i / 32] & (1 << (i % 32)))) {
                memory_bitmap[i / 32] |= (1 << (i % 32));
                return (void*)(i * PAGE_SIZE);
            }
        }
        return nullptr; // Out of memory
    }

    // Free a page of memory
    void free_page(void* ptr) {
        uint32_t page = (uint32_t)ptr / PAGE_SIZE;
        memory_bitmap[page / 32] &= ~(1 << (page % 32));
    }

    // Map a physical address to a virtual address
    void map_page(uint32_t physical_addr, uint32_t virtual_addr) {
        uint32_t page_idx = virtual_addr / PAGE_SIZE;
        uint32_t table_idx = page_idx / 1024;
        uint32_t page_offset = page_idx % 1024;

        // Get or create page table
        PageTable* table;
        if (!current_directory->entries[table_idx].present) {
            table = (PageTable*)allocate_page();
            current_directory->entries[table_idx].present = 1;
            current_directory->entries[table_idx].rw = 1;
            current_directory->entries[table_idx].user = 0;
            current_directory->entries[table_idx].frame = (uint32_t)table >> 12;
        } else {
            table = (PageTable*)(current_directory->entries[table_idx].frame << 12);
        }

        // Map the page
        table->entries[page_offset].present = 1;
        table->entries[page_offset].rw = 1;
        table->entries[page_offset].user = 0;
        table->entries[page_offset].frame = physical_addr >> 12;
    }

    // Get total available memory
    size_t get_total_memory() {
        return total_pages * PAGE_SIZE;
    }

    // Get free memory
    size_t get_free_memory() {
        size_t free_pages = 0;
        for (size_t i = 0; i < total_pages; i++) {
            if (!(memory_bitmap[i / 32] & (1 << (i % 32)))) {
                free_pages++;
            }
        }
        return free_pages * PAGE_SIZE;
    }

    // Internal function to switch page directory
    static void switch_page_directory(PageDirectory* dir) {
        current_directory = dir;
        asm volatile("mov %0, %%cr3":: "r"(dir));
        uint32_t cr0;
        asm volatile("mov %%cr0, %0": "=r"(cr0));
        cr0 |= 0x80000000;
        asm volatile("mov %0, %%cr0":: "r"(cr0));
    }
}; 