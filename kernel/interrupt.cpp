#include "interrupt.h"
#include <stdint.h>

namespace Interrupt {
    // Interrupt Descriptor Table (IDT) entry
    struct IDTEntry {
        uint16_t base_low;
        uint16_t selector;
        uint8_t zero;
        uint8_t flags;
        uint16_t base_high;
    } __attribute__((packed));

    // IDT pointer structure
    struct IDTPointer {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed));

    // Global variables
    static IDTEntry idt[256];
    static IDTPointer idt_pointer;
    static InterruptHandler handlers[256];
    static bool interrupts_enabled = false;

    // Assembly function to load IDT
    extern "C" void load_idt(uint32_t idt_pointer);

    // Initialize interrupt handling
    void init() {
        // Set up IDT pointer
        idt_pointer.limit = (sizeof(IDTEntry) * 256) - 1;
        idt_pointer.base = (uint32_t)&idt;

        // Clear out the entire IDT
        for (int i = 0; i < 256; i++) {
            handlers[i] = nullptr;
            idt[i].base_low = 0;
            idt[i].selector = 0;
            idt[i].zero = 0;
            idt[i].flags = 0;
            idt[i].base_high = 0;
        }

        // Load the IDT
        load_idt((uint32_t)&idt_pointer);

        // Enable interrupts
        enable();
    }

    // Register an interrupt handler
    void register_handler(uint8_t interrupt_number, InterruptHandler handler) {
        handlers[interrupt_number] = handler;
        
        // Set up the IDT entry
        uint32_t base = (uint32_t)handler;
        idt[interrupt_number].base_low = base & 0xFFFF;
        idt[interrupt_number].selector = 0x08; // Kernel code segment
        idt[interrupt_number].zero = 0;
        idt[interrupt_number].flags = 0x8E; // Present, Ring 0, 32-bit Interrupt Gate
        idt[interrupt_number].base_high = (base >> 16) & 0xFFFF;
    }

    // Enable interrupts
    void enable() {
        asm volatile("sti");
        interrupts_enabled = true;
    }

    // Disable interrupts
    void disable() {
        asm volatile("cli");
        interrupts_enabled = false;
    }

    // Check if interrupts are enabled
    bool are_enabled() {
        return interrupts_enabled;
    }

    // Common interrupt handlers
    void handle_division_by_zero(uint32_t interrupt_number, uint32_t error_code) {
        // Handle division by zero error
        // TODO: Implement proper error handling
        while(1) {
            __asm__("hlt");
        }
    }

    void handle_page_fault(uint32_t interrupt_number, uint32_t error_code) {
        // Handle page fault
        // TODO: Implement proper page fault handling
        while(1) {
            __asm__("hlt");
        }
    }

    void handle_keyboard(uint32_t interrupt_number, uint32_t error_code) {
        // Read keyboard input
        uint8_t scancode = inb(0x60);
        // TODO: Implement proper keyboard handling
        outb(0x20, 0x20); // Send EOI to PIC
    }

    void handle_timer(uint32_t interrupt_number, uint32_t error_code) {
        // Handle timer interrupt
        // TODO: Implement proper timer handling
        outb(0x20, 0x20); // Send EOI to PIC
    }

    // Helper functions for I/O
    static inline uint8_t inb(uint16_t port) {
        uint8_t ret;
        asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
    }

    static inline void outb(uint16_t port, uint8_t val) {
        asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
    }
}; 