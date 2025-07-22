#include <stdint.h>
#include "memory.h"
#include "interrupt.h"
#include "filesystem.h"
#include "process.h"

// Function to write a character to the screen
void putchar(char c) {
    // Video memory address
    volatile uint16_t* video_memory = (uint16_t*)0xB8000;
    static int cursor_x = 0;
    static int cursor_y = 0;

    // Handle newline
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        return;
    }

    // Write character with white-on-black attribute
    const uint16_t attribute = 0x0F00;
    video_memory[cursor_y * 80 + cursor_x] = (uint16_t)c | attribute;

    // Update cursor position
    cursor_x++;
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }
}

// Function to print a string
void print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putchar(str[i]);
    }
}

// Function to print a number
void print_number(uint32_t num) {
    char buffer[32];
    int i = 0;
    
    if (num == 0) {
        putchar('0');
        return;
    }
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (--i >= 0) {
        putchar(buffer[i]);
    }
}

// Example process entry point
void example_process() {
    while (1) {
        print("Example process running...\n");
        // Simple delay
        for (volatile int i = 0; i < 1000000; i++) {}
    }
}

// Kernel entry point
extern "C" void kernel_main() {
    // Clear the screen
    volatile uint16_t* video_memory = (uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i] = (uint16_t)' ' | (uint16_t)0x0F00;
    }

    print("Initializing SimpleOS...\n");

    // Initialize memory management
    print("Initializing memory management...\n");
    Memory::init();
    print("Memory management initialized.\n");

    // Initialize interrupt handling
    print("Initializing interrupt handling...\n");
    Interrupt::init();
    print("Interrupt handling initialized.\n");

    // Initialize file system
    print("Initializing file system...\n");
    FileSystem::init();
    print("File system initialized.\n");

    // Initialize process management
    print("Initializing process management...\n");
    Process::init();
    print("Process management initialized.\n");

    // Create example process
    print("Creating example process...\n");
    uint32_t pid = Process::create_process(example_process);
    print("Example process created with PID: ");
    print_number(pid);
    print("\n");

    print("Kernel initialization complete.\n");
    print("Starting process scheduler...\n");

    // Start process scheduler
    while (1) {
        Process::schedule();
        // Simple delay
        for (volatile int i = 0; i < 1000000; i++) {}
    }
} 