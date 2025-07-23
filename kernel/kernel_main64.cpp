/**
 * @file kernel_main64.cpp
 * @brief 64-bit kernel main entry point
 * 
 * This file contains the main C++ entry point for the 64-bit kernel.
 * It's called from entry64.asm after basic assembly initialization.
 */

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string_view>

// Include our modernized headers
#include "../include/memory.h"
#include "../include/interrupt.h" 
#include "../include/process.h"
#include "../include/filesystem.h"
#include "../include/shell.h"

// UEFI boot configuration structure (matching bootloader)
struct BootConfig {
    std::uint64_t kernel_virtual_base;
    std::uint64_t kernel_physical_base;
    std::uint64_t kernel_size;
    std::uint64_t stack_top;
    std::uint64_t heap_start;
    std::uint64_t heap_size;
    void* memory_map;
    std::size_t memory_map_size;
    std::size_t memory_map_key;
    std::size_t descriptor_size;
    std::uint32_t descriptor_version;
    void* graphics_info;
    void* framebuffer_base;
    std::size_t framebuffer_size;
};

// Forward declarations
extern "C" {
    void kernel_main64(BootConfig* boot_config) noexcept;
    void handle_exception(void* interrupt_frame) noexcept;
    std::uint64_t handle_syscall(std::uint64_t syscall_num, 
                                std::uint64_t arg1, std::uint64_t arg2,
                                std::uint64_t arg3, std::uint64_t arg4,
                                std::uint64_t arg5, std::uint64_t arg6) noexcept;
    void load_gdt64() noexcept;
    void load_idt64(const void* idt_ptr) noexcept;
}

// Global kernel state
namespace Kernel {
    static BootConfig* g_boot_config = nullptr;
    static bool g_initialized = false;
    static std::uint64_t g_kernel_start_time = 0;
    
    // Simple VGA text mode for early output
    constexpr std::uint16_t* VGA_MEMORY = reinterpret_cast<std::uint16_t*>(0xFFFFFFFF800B8000ULL);
    constexpr std::size_t VGA_WIDTH = 80;
    constexpr std::size_t VGA_HEIGHT = 25;
    
    static std::size_t g_cursor_x = 0;
    static std::size_t g_cursor_y = 0;
    static std::uint8_t g_color = 0x07; // Light gray on black
}

// Simple console output functions
namespace Console {
    
    void clear_screen() noexcept {
        const std::uint16_t blank = (Kernel::g_color << 8) | ' ';
        for (std::size_t i = 0; i < Kernel::VGA_WIDTH * Kernel::VGA_HEIGHT; ++i) {
            Kernel::VGA_MEMORY[i] = blank;
        }
        Kernel::g_cursor_x = 0;
        Kernel::g_cursor_y = 0;
    }
    
    void scroll_up() noexcept {
        // Move all lines up by one
        for (std::size_t y = 1; y < Kernel::VGA_HEIGHT; ++y) {
            for (std::size_t x = 0; x < Kernel::VGA_WIDTH; ++x) {
                const std::size_t src = y * Kernel::VGA_WIDTH + x;
                const std::size_t dst = (y - 1) * Kernel::VGA_WIDTH + x;
                Kernel::VGA_MEMORY[dst] = Kernel::VGA_MEMORY[src];
            }
        }
        
        // Clear the last line
        const std::uint16_t blank = (Kernel::g_color << 8) | ' ';
        const std::size_t last_line_start = (Kernel::VGA_HEIGHT - 1) * Kernel::VGA_WIDTH;
        for (std::size_t x = 0; x < Kernel::VGA_WIDTH; ++x) {
            Kernel::VGA_MEMORY[last_line_start + x] = blank;
        }
        
        Kernel::g_cursor_y = Kernel::VGA_HEIGHT - 1;
    }
    
    void putchar(char c) noexcept {
        if (c == '\n') {
            Kernel::g_cursor_x = 0;
            ++Kernel::g_cursor_y;
        } else if (c == '\r') {
            Kernel::g_cursor_x = 0;
        } else if (c == '\t') {
            Kernel::g_cursor_x = (Kernel::g_cursor_x + 8) & ~7;
        } else if (c >= 32) { // Printable character
            if (Kernel::g_cursor_x >= Kernel::VGA_WIDTH) {
                Kernel::g_cursor_x = 0;
                ++Kernel::g_cursor_y;
            }
            
            const std::size_t index = Kernel::g_cursor_y * Kernel::VGA_WIDTH + Kernel::g_cursor_x;
            Kernel::VGA_MEMORY[index] = (Kernel::g_color << 8) | c;
            ++Kernel::g_cursor_x;
        }
        
        // Handle scrolling
        if (Kernel::g_cursor_y >= Kernel::VGA_HEIGHT) {
            scroll_up();
        }
    }
    
    void print(std::string_view str) noexcept {
        for (char c : str) {
            putchar(c);
        }
    }
    
    void print_hex(std::uint64_t value) noexcept {
        constexpr char hex_chars[] = "0123456789ABCDEF";
        char buffer[19] = "0x";
        
        for (int i = 15; i >= 0; --i) {
            buffer[i + 2] = hex_chars[(value >> (i * 4)) & 0xF];
        }
        buffer[18] = '\0';
        
        print(buffer);
    }
    
    void print_dec(std::uint64_t value) noexcept {
        if (value == 0) {
            putchar('0');
            return;
        }
        
        char buffer[21]; // Enough for 64-bit number
        int pos = 20;
        buffer[pos] = '\0';
        
        while (value > 0) {
            buffer[--pos] = '0' + (value % 10);
            value /= 10;
        }
        
        print(buffer + pos);
    }
    
    void set_color(std::uint8_t color) noexcept {
        Kernel::g_color = color;
    }
}

// Memory management initialization
namespace KernelMemory {
    Memory::AllocResult<void> initialize_from_uefi(BootConfig* config) noexcept {
        Console::print("  Parsing UEFI memory map...\n");
        
        // Convert UEFI memory map to our format
        const auto* uefi_map = static_cast<const char*>(config->memory_map);
        const std::size_t entry_count = config->memory_map_size / config->descriptor_size;
        
        Console::print("  Found ");
        Console::print_dec(entry_count);
        Console::print(" memory descriptors\n");
        
        // Create span of memory map entries for our memory system
        std::vector<Memory::MemoryMapEntry> memory_entries;
        memory_entries.reserve(entry_count);
        
        for (std::size_t i = 0; i < entry_count; ++i) {
            // UEFI memory descriptor (simplified)
            struct UEFIMemoryDescriptor {
                std::uint32_t type;
                std::uint64_t physical_start;
                std::uint64_t virtual_start;
                std::uint64_t number_of_pages;
                std::uint64_t attribute;
            };
            
            const auto* desc = reinterpret_cast<const UEFIMemoryDescriptor*>(
                uefi_map + i * config->descriptor_size);
            
            Memory::MemoryMapEntry entry{};
            entry.base_addr = desc->physical_start;
            entry.length = desc->number_of_pages * 4096; // EFI page size
            
            // Map UEFI memory types to our types
            switch (desc->type) {
                case 7: // EfiConventionalMemory
                    entry.type = Memory::MemoryType::EfiConventionalMemory;
                    break;
                case 3: // EfiBootServicesCode
                    entry.type = Memory::MemoryType::EfiBootServicesCode;
                    break;
                case 4: // EfiBootServicesData
                    entry.type = Memory::MemoryType::EfiBootServicesData;
                    break;
                default:
                    entry.type = Memory::MemoryType::EfiReservedMemoryType;
                    break;
            }
            
            entry.extended_attributes = static_cast<Memory::MemoryAttributes>(desc->attribute);
            memory_entries.push_back(entry);
        }
        
        // Initialize our memory management system
        std::span<const Memory::MemoryMapEntry> memory_span(memory_entries);
        return Memory::init(memory_span);
    }
}

// Exception handling
extern "C" void handle_exception(void* interrupt_frame) noexcept {
    Console::set_color(0x0C); // Light red
    Console::print("\n*** KERNEL PANIC ***\n");
    Console::print("Exception occurred at: ");
    Console::print_hex(reinterpret_cast<std::uintptr_t>(interrupt_frame));
    Console::print("\n");
    Console::print("System halted.\n");
    
    // Halt the system
    asm volatile("cli; hlt");
    while (true) {
        asm volatile("hlt");
    }
}

// Modern system call handling
extern "C" std::uint64_t handle_syscall(std::uint64_t syscall_num, 
                                       std::uint64_t arg1, std::uint64_t arg2,
                                       std::uint64_t arg3, std::uint64_t arg4,
                                       std::uint64_t arg5, std::uint64_t arg6) noexcept {
    // System call numbers (Linux-compatible)
    enum SystemCall : std::uint64_t {
        SYS_READ = 0,
        SYS_WRITE = 1,
        SYS_OPEN = 2,
        SYS_CLOSE = 3,
        SYS_STAT = 4,
        SYS_FSTAT = 5,
        SYS_LSTAT = 6,
        SYS_POLL = 7,
        SYS_LSEEK = 8,
        SYS_MMAP = 9,
        SYS_MPROTECT = 10,
        SYS_MUNMAP = 11,
        SYS_BRK = 12,
        SYS_RT_SIGACTION = 13,
        SYS_RT_SIGPROCMASK = 14,
        SYS_RT_SIGRETURN = 15,
        SYS_IOCTL = 16,
        SYS_PREAD64 = 17,
        SYS_PWRITE64 = 18,
        SYS_READV = 19,
        SYS_WRITEV = 20,
        SYS_ACCESS = 21,
        SYS_PIPE = 22,
        SYS_SELECT = 23,
        SYS_SCHED_YIELD = 24,
        SYS_MREMAP = 25,
        SYS_MSYNC = 26,
        SYS_MINCORE = 27,
        SYS_MADVISE = 28,
        SYS_SHMGET = 29,
        SYS_SHMAT = 30,
        SYS_SHMCTL = 31,
        SYS_DUP = 32,
        SYS_DUP2 = 33,
        SYS_PAUSE = 34,
        SYS_NANOSLEEP = 35,
        SYS_GETITIMER = 36,
        SYS_ALARM = 37,
        SYS_SETITIMER = 38,
        SYS_GETPID = 39,
        SYS_SENDFILE = 40,
        SYS_SOCKET = 41,
        SYS_CONNECT = 42,
        SYS_ACCEPT = 43,
        SYS_SENDTO = 44,
        SYS_RECVFROM = 45,
        SYS_SENDMSG = 46,
        SYS_RECVMSG = 47,
        SYS_SHUTDOWN = 48,
        SYS_BIND = 49,
        SYS_LISTEN = 50,
        SYS_GETSOCKNAME = 51,
        SYS_GETPEERNAME = 52,
        SYS_SOCKETPAIR = 53,
        SYS_SETSOCKOPT = 54,
        SYS_GETSOCKOPT = 55,
        SYS_CLONE = 56,
        SYS_FORK = 57,
        SYS_VFORK = 58,
        SYS_EXECVE = 59,
        SYS_EXIT = 60,
        SYS_WAIT4 = 61,
        SYS_KILL = 62,
        SYS_UNAME = 63,
        SYS_SEMGET = 64,
        SYS_SEMOP = 65,
        SYS_SEMCTL = 66,
        SYS_SHMDT = 67,
        SYS_MSGGET = 68,
        SYS_MSGSND = 69,
        SYS_MSGRCV = 70,
        SYS_MSGCTL = 71,
        SYS_FCNTL = 72,
        SYS_FLOCK = 73,
        SYS_FSYNC = 74,
        SYS_FDATASYNC = 75,
        SYS_TRUNCATE = 76,
        SYS_FTRUNCATE = 77,
        SYS_GETDENTS = 78,
        SYS_GETCWD = 79,
        SYS_CHDIR = 80,
        SYS_FCHDIR = 81,
        SYS_RENAME = 82,
        SYS_MKDIR = 83,
        SYS_RMDIR = 84,
        SYS_CREAT = 85,
        SYS_LINK = 86,
        SYS_UNLINK = 87,
        SYS_SYMLINK = 88,
        SYS_READLINK = 89,
        SYS_CHMOD = 90,
        SYS_FCHMOD = 91,
        SYS_CHOWN = 92,
        SYS_FCHOWN = 93,
        SYS_LCHOWN = 94,
        SYS_UMASK = 95,
        SYS_GETTIMEOFDAY = 96,
        SYS_GETRLIMIT = 97,
        SYS_GETRUSAGE = 98,
        SYS_SYSINFO = 99,
        SYS_TIMES = 100
    };
    
    switch (syscall_num) {
        case SYS_EXIT: {
            // Exit current process
            Process::SystemCalls::sys_exit(static_cast<int>(arg1));
            return 0; // Never reached
        }
        
        case SYS_WRITE: {
            // Write to file descriptor
            auto fd = static_cast<FileSystem::FileDescriptor>(arg1);
            const char* buffer = reinterpret_cast<const char*>(arg2);
            auto count = static_cast<std::size_t>(arg3);
            
            if (fd == FileSystem::STDOUT_FD || fd == FileSystem::STDERR_FD) {
                // Write to console
                for (std::size_t i = 0; i < count; ++i) {
                    Console::putchar(buffer[i]);
                }
                return count;
            } else {
                // TODO: Implement file descriptor write
                return -1; // EBADF
            }
        }
        
        case SYS_READ: {
            // Read from file descriptor
            auto fd = static_cast<FileSystem::FileDescriptor>(arg1);
            char* buffer = reinterpret_cast<char*>(arg2);
            auto count = static_cast<std::size_t>(arg3);
            
            if (fd == FileSystem::STDIN_FD) {
                // TODO: Implement keyboard input
                return 0;
            } else {
                // TODO: Implement file descriptor read
                return -1; // EBADF
            }
        }
        
        case SYS_OPEN: {
            // Open file
            const char* pathname = reinterpret_cast<const char*>(arg1);
            auto flags = static_cast<int>(arg2);
            auto mode = static_cast<FileSystem::FilePermissions>(arg3);
            
            // Convert flags to AccessMode
            FileSystem::AccessMode access_mode = FileSystem::AccessMode::ReadOnly;
            if (flags & 1) access_mode = FileSystem::AccessMode::WriteOnly;
            if (flags & 2) access_mode = FileSystem::AccessMode::ReadWrite;
            if (flags & 64) access_mode = FileSystem::AccessMode::Create;
            
            auto file_result = FileSystem::open(pathname, access_mode);
            if (file_result.has_value()) {
                return file_result.value().descriptor();
            } else {
                return -1; // Error
            }
        }
        
        case SYS_CLOSE: {
            // Close file descriptor
            auto fd = static_cast<FileSystem::FileDescriptor>(arg1);
            // TODO: Implement proper file descriptor closing
            return 0;
        }
        
        case SYS_GETPID: {
            // Get process ID
            return Process::SystemCalls::sys_getpid();
        }
        
        case SYS_GETPPID: {
            // Get parent process ID
            return Process::SystemCalls::sys_getppid();
        }
        
        case SYS_FORK: {
            // Fork process
            auto fork_result = Process::SystemCalls::sys_fork();
            if (fork_result.has_value()) {
                return fork_result.value();
            } else {
                return -1;
            }
        }
        
        case SYS_MKDIR: {
            // Create directory
            const char* pathname = reinterpret_cast<const char*>(arg1);
            auto mode = static_cast<FileSystem::FilePermissions>(arg2);
            
            auto result = FileSystem::create_directory(pathname, mode, false);
            return result.has_value() ? 0 : -1;
        }
        
        case SYS_RMDIR: {
            // Remove directory
            const char* pathname = reinterpret_cast<const char*>(arg1);
            // TODO: Implement directory removal
            return -1;
        }
        
        case SYS_CHDIR: {
            // Change directory
            const char* pathname = reinterpret_cast<const char*>(arg1);
            auto result = FileSystem::set_current_directory(pathname);
            return result.has_value() ? 0 : -1;
        }
        
        case SYS_GETCWD: {
            // Get current working directory
            char* buffer = reinterpret_cast<char*>(arg1);
            auto size = static_cast<std::size_t>(arg2);
            
            auto cwd = FileSystem::get_current_directory();
            auto cwd_str = cwd.string();
            
            if (cwd_str.length() + 1 <= size) {
                std::strcpy(buffer, cwd_str.c_str());
                return reinterpret_cast<std::uint64_t>(buffer);
            } else {
                return 0; // ERANGE
            }
        }
        
        case SYS_ACCESS: {
            // Check file accessibility
            const char* pathname = reinterpret_cast<const char*>(arg1);
            auto mode = static_cast<int>(arg2);
            
            return FileSystem::exists(pathname) ? 0 : -1;
        }
        
        case SYS_SCHED_YIELD: {
            // Yield CPU to other processes
            auto result = Process::yield();
            return result.has_value() ? 0 : -1;
        }
        
        case SYS_NANOSLEEP: {
            // Sleep for specified time
            // TODO: Implement proper sleep with timespec
            Process::sleep(std::chrono::milliseconds(100));
            return 0;
        }
        
        default:
            Console::print("Unknown system call: ");
            Console::print_dec(syscall_num);
            Console::print("\n");
            return -1; // ENOSYS
    }
}

// System test processes
namespace TestProcesses {
    
    void test_memory_allocation() noexcept {
        Console::print("Testing memory allocation...\n");
        
        // Test page allocation
        auto page_result = Memory::allocate_page();
        if (page_result.has_value()) {
            Console::print("  Page allocated at: ");
            Console::print_hex(page_result.value());
            Console::print("\n");
            
            // Free the page
            auto free_result = Memory::free_page(page_result.value());
            if (free_result.has_value()) {
                Console::print("  Page freed successfully\n");
            } else {
                Console::print("  Failed to free page\n");
            }
        } else {
            Console::print("  Failed to allocate page\n");
        }
    }
    
    void test_filesystem() noexcept {
        Console::print("Testing filesystem...\n");
        
        // Test directory creation
        auto mkdir_result = FileSystem::create_directory("/test", FileSystem::FilePermissions::Default, false);
        if (mkdir_result.has_value()) {
            Console::print("  Directory created successfully\n");
        } else {
            Console::print("  Failed to create directory\n");
        }
        
        // Test file creation
        auto create_result = FileSystem::create_file("/test/hello.txt", FileSystem::FilePermissions::Default);
        if (create_result.has_value()) {
            Console::print("  File created successfully\n");
            
            // Test file write
            auto write_result = FileSystem::Convenience::write_text_file("/test/hello.txt", "Hello ModernOS!", true);
            if (write_result.has_value()) {
                Console::print("  File written successfully\n");
                
                // Test file read
                auto read_result = FileSystem::Convenience::read_text_file("/test/hello.txt");
                if (read_result.has_value()) {
                    Console::print("  File content: ");
                    Console::print(read_result.value().c_str());
                    Console::print("\n");
                } else {
                    Console::print("  Failed to read file\n");
                }
            } else {
                Console::print("  Failed to write file\n");
            }
        } else {
            Console::print("  Failed to create file\n");
        }
    }
    
    void test_process_creation() noexcept {
        Console::print("Testing process creation...\n");
        
        // Simple lambda for test process
        auto test_proc = []() {
            Console::print("  Hello from test process!\n");
            // Test filesystem from within process
            auto cwd = FileSystem::get_current_directory();
            Console::print("  Current directory: ");
            Console::print(cwd.string().c_str());
            Console::print("\n");
        };
        
        Process::ProcessCreateInfo create_info{};
        create_info.name = "test_process";
        create_info.priority = Process::ProcessPriority::Normal;
        
        auto proc_result = Process::create_process(test_proc, create_info);
        if (proc_result.has_value()) {
            Console::print("  Process created successfully with PID ");
            Console::print_dec(proc_result.value().get());
            Console::print("\n");
        } else {
            Console::print("  Failed to create process\n");
        }
    }
    
    void run_all_tests() noexcept {
        Console::print("\n=== Running System Tests ===\n");
        test_memory_allocation();
        test_filesystem();
        test_process_creation();
        Console::print("=== Tests Complete ===\n\n");
    }
}

// Main kernel initialization and execution
extern "C" void kernel_main64(BootConfig* boot_config) noexcept {
    // Store boot configuration
    Kernel::g_boot_config = boot_config;
    
    // Initialize console
    Console::clear_screen();
    Console::set_color(0x0F); // Bright white
    
    // Display welcome message
    Console::print("========================================\n");
    Console::print("       ModernOS 64-bit Kernel\n");
    Console::print("========================================\n");
    Console::print("Version: 1.0.0-alpha\n");
    Console::print("Architecture: x86_64\n");
    Console::print("Boot: UEFI\n");
    Console::print("========================================\n\n");
    
    // Display boot configuration
    Console::print("Boot Configuration:\n");
    Console::print("  Kernel Physical: ");
    Console::print_hex(boot_config->kernel_physical_base);
    Console::print("\n");
    Console::print("  Kernel Virtual:  ");
    Console::print_hex(boot_config->kernel_virtual_base);
    Console::print("\n");
    Console::print("  Kernel Size:     ");
    Console::print_dec(boot_config->kernel_size);
    Console::print(" bytes\n");
    Console::print("  Memory Map:      ");
    Console::print_dec(boot_config->memory_map_size / boot_config->descriptor_size);
    Console::print(" entries\n");
    
    if (boot_config->framebuffer_base) {
        Console::print("  Framebuffer:     ");
        Console::print_hex(reinterpret_cast<std::uintptr_t>(boot_config->framebuffer_base));
        Console::print("\n");
    }
    Console::print("\n");
    
    // Initialize kernel subsystems
    Console::set_color(0x0A); // Light green
    Console::print("Initializing kernel subsystems...\n");
    
    // 1. Initialize memory management
    Console::print("[1/4] Memory management...\n");
    auto memory_result = KernelMemory::initialize_from_uefi(boot_config);
    if (!memory_result.has_value()) {
        Console::set_color(0x0C); // Light red
        Console::print("FATAL: Memory initialization failed\n");
        goto kernel_panic;
    }
    Console::print("  Memory management initialized\n");
    
    // 2. Initialize interrupt handling
    Console::print("[2/4] Interrupt handling...\n");
    auto interrupt_result = Interrupt::init();
    if (!interrupt_result.has_value()) {
        Console::set_color(0x0C);
        Console::print("FATAL: Interrupt initialization failed\n");
        goto kernel_panic;
    }
    Console::print("  Interrupt handling initialized\n");
    
    // 3. Initialize file system
    Console::print("[3/4] File system...\n");
    auto fs_result = FileSystem::init();
    if (!fs_result.has_value()) {
        Console::set_color(0x0C);
        Console::print("FATAL: File system initialization failed\n");
        goto kernel_panic;
    }
    Console::print("  File system initialized\n");
    
    // 4. Initialize process management
    Console::print("[4/4] Process management...\n");
    auto process_result = Process::init();
    if (!process_result.has_value()) {
        Console::set_color(0x0C);
        Console::print("FATAL: Process management initialization failed\n");
        goto kernel_panic;
    }
    Console::print("  Process management initialized\n");
    
    // Mark kernel as fully initialized
    Kernel::g_initialized = true;
    
    Console::set_color(0x0F); // Bright white
    Console::print("\nKernel initialization complete!\n");
    
    // Run tests
    TestProcesses::run_all_tests();
    
    // Enable interrupts
    Console::print("Enabling interrupts...\n");
    asm volatile("sti");
    
    // Start interactive shell
    Console::print("Starting interactive shell...\n");
    auto shell_result = Shell::start_shell();
    if (shell_result.has_value()) {
        Console::print("Shell started with PID ");
        Console::print_dec(shell_result.value().get());
        Console::print("\n");
    } else {
        Console::print("Failed to start shell\n");
    }
    
    // Main kernel loop
    Console::print("Entering main kernel loop...\n");
    Console::print("ModernOS is now running!\n\n");
    
    std::uint64_t loop_count = 0;
    while (true) {
        // Kernel idle loop with less frequent output
        if ((loop_count % 10000000) == 0) {
            // Show system status periodically
            auto proc_count = Process::get_process_count();
            if (proc_count > 1) { // More than just kernel process
                Console::set_color(0x08); // Dark gray
                Console::print("[");
                Console::print_dec(proc_count);
                Console::print(" processes] ");
                Console::set_color(0x0F); // White
            }
        }
        
        // Yield to other processes
        Process::yield();
        
        // Simple power management - halt until next interrupt
        asm volatile("hlt");
        
        ++loop_count;
    }
    
kernel_panic:
    Console::set_color(0x0C); // Light red
    Console::print("\n*** KERNEL PANIC ***\n");
    Console::print("System initialization failed.\n");
    Console::print("System halted.\n");
    
    // Disable interrupts and halt
    asm volatile("cli");
    while (true) {
        asm volatile("hlt");
    }
}

// Kernel utility functions
namespace Kernel {
    
    bool is_initialized() noexcept {
        return g_initialized;
    }
    
    BootConfig* get_boot_config() noexcept {
        return g_boot_config;
    }
    
    std::uint64_t get_uptime() noexcept {
        // TODO: Implement proper time tracking
        return 0;
    }
    
    void panic(std::string_view message) noexcept {
        Console::set_color(0x0C); // Light red
        Console::print("\n*** KERNEL PANIC ***\n");
        Console::print(message);
        Console::print("\nSystem halted.\n");
        
        asm volatile("cli");
        while (true) {
            asm volatile("hlt");
        }
    }
}