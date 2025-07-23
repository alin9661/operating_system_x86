/**
 * @file interrupt.cpp
 * @brief Modern C++20 interrupt handling implementation
 * 
 * This file implements the 64-bit interrupt management system using C++20 features
 * and modern x86_64 interrupt handling.
 */

#include "../include/interrupt.h"
#include "../include/process.h"
#include "../include/memory.h"
#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace Interrupt {
    // Internal interrupt management state
    namespace Internal {
        // IDT and interrupt management
        static alignas(16) std::array<IDTEntry, IDT_ENTRIES> idt_table;
        static IDTPointer idt_ptr;
        static std::array<std::function<void(InterruptFrame&)>, IDT_ENTRIES> handlers;
        static std::mutex handler_mutex;
        
        // Interrupt statistics
        static InterruptStatistics stats;
        
        // System initialization flag
        static std::atomic<bool> initialized{false};
        
        // Current interrupt controller
        static std::unique_ptr<InterruptController> controller;
        
        // Exception handler registry
        static std::unordered_map<std::uint8_t, std::unique_ptr<ExceptionHandler>> exception_handlers;
        
        // Assembly interrupt stubs - these would be defined in assembly files
        extern "C" {
            void isr_common_stub();
            void irq_common_stub();
            void idt_flush(std::uint64_t idt_ptr);
            
            // Exception ISR stubs (0-31)
            void isr0(); void isr1(); void isr2(); void isr3(); void isr4();
            void isr5(); void isr6(); void isr7(); void isr8(); void isr9();
            void isr10(); void isr11(); void isr12(); void isr13(); void isr14();
            void isr15(); void isr16(); void isr17(); void isr18(); void isr19();
            void isr20(); void isr21(); void isr22(); void isr23(); void isr24();
            void isr25(); void isr26(); void isr27(); void isr28(); void isr29();
            void isr30(); void isr31();
            
            // IRQ stubs (32-47)
            void irq0(); void irq1(); void irq2(); void irq3(); void irq4();
            void irq5(); void irq6(); void irq7(); void irq8(); void irq9();
            void irq10(); void irq11(); void irq12(); void irq13(); void irq14();
            void irq15();
        }
        
        // Common interrupt handler dispatcher
        extern "C" void interrupt_handler(InterruptFrame* frame) {
            if (!frame) return;
            
            stats.total_interrupts.fetch_add(1);
            
            std::uint8_t vector = frame->vector;
            
            // Update vector-specific statistics
            if (vector < IDT_ENTRIES) {
                stats.vector_counts[vector].fetch_add(1);
            }
            
            // Classify interrupt type
            if (is_exception(vector)) {
                stats.exceptions.fetch_add(1);
                
                // Call exception handler if registered
                auto it = exception_handlers.find(vector);
                if (it != exception_handlers.end()) {
                    it->second->handle(*frame);
                } else {
                    // Default exception handling
                    Exceptions::handle_generic_exception(*frame);
                }
            } else if (is_hardware_irq(vector)) {
                stats.hardware_interrupts.fetch_add(1);
                
                // Call registered handler
                auto& handler = handlers[vector];
                if (handler) {
                    handler(*frame);
                }
                
                // Send EOI
                if (controller) {
                    controller->send_eoi(vector - USER_INTERRUPT_BASE);
                }
            } else {
                stats.software_interrupts.fetch_add(1);
                
                // Software interrupt or system call
                auto& handler = handlers[vector];
                if (handler) {
                    handler(*frame);
                }
            }
        }
        
        // Set up IDT entry
        void set_idt_entry(std::uint8_t index, std::uint64_t handler_addr, 
                          GateType gate_type, PrivilegeLevel dpl) {
            idt_table[index].set_gate(handler_addr, 0x08, gate_type, dpl, true);
        }
        
        // Initialize exception ISRs
        void setup_exception_isrs() {
            // Exception handlers (0-31)
            set_idt_entry(0, reinterpret_cast<std::uint64_t>(isr0), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(1, reinterpret_cast<std::uint64_t>(isr1), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(2, reinterpret_cast<std::uint64_t>(isr2), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(3, reinterpret_cast<std::uint64_t>(isr3), GateType::TrapGate, PrivilegeLevel::User);
            set_idt_entry(4, reinterpret_cast<std::uint64_t>(isr4), GateType::TrapGate, PrivilegeLevel::User);
            set_idt_entry(5, reinterpret_cast<std::uint64_t>(isr5), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(6, reinterpret_cast<std::uint64_t>(isr6), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(7, reinterpret_cast<std::uint64_t>(isr7), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(8, reinterpret_cast<std::uint64_t>(isr8), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(9, reinterpret_cast<std::uint64_t>(isr9), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(10, reinterpret_cast<std::uint64_t>(isr10), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(11, reinterpret_cast<std::uint64_t>(isr11), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(12, reinterpret_cast<std::uint64_t>(isr12), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(13, reinterpret_cast<std::uint64_t>(isr13), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(14, reinterpret_cast<std::uint64_t>(isr14), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(15, reinterpret_cast<std::uint64_t>(isr15), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(16, reinterpret_cast<std::uint64_t>(isr16), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(17, reinterpret_cast<std::uint64_t>(isr17), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(18, reinterpret_cast<std::uint64_t>(isr18), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(19, reinterpret_cast<std::uint64_t>(isr19), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(20, reinterpret_cast<std::uint64_t>(isr20), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(21, reinterpret_cast<std::uint64_t>(isr21), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(22, reinterpret_cast<std::uint64_t>(isr22), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(23, reinterpret_cast<std::uint64_t>(isr23), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(24, reinterpret_cast<std::uint64_t>(isr24), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(25, reinterpret_cast<std::uint64_t>(isr25), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(26, reinterpret_cast<std::uint64_t>(isr26), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(27, reinterpret_cast<std::uint64_t>(isr27), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(28, reinterpret_cast<std::uint64_t>(isr28), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(29, reinterpret_cast<std::uint64_t>(isr29), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(30, reinterpret_cast<std::uint64_t>(isr30), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(31, reinterpret_cast<std::uint64_t>(isr31), GateType::InterruptGate, PrivilegeLevel::Kernel);
        }
        
        // Initialize IRQ handlers
        void setup_irq_handlers() {
            // IRQ handlers (32-47)
            set_idt_entry(32, reinterpret_cast<std::uint64_t>(irq0), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(33, reinterpret_cast<std::uint64_t>(irq1), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(34, reinterpret_cast<std::uint64_t>(irq2), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(35, reinterpret_cast<std::uint64_t>(irq3), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(36, reinterpret_cast<std::uint64_t>(irq4), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(37, reinterpret_cast<std::uint64_t>(irq5), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(38, reinterpret_cast<std::uint64_t>(irq6), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(39, reinterpret_cast<std::uint64_t>(irq7), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(40, reinterpret_cast<std::uint64_t>(irq8), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(41, reinterpret_cast<std::uint64_t>(irq9), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(42, reinterpret_cast<std::uint64_t>(irq10), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(43, reinterpret_cast<std::uint64_t>(irq11), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(44, reinterpret_cast<std::uint64_t>(irq12), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(45, reinterpret_cast<std::uint64_t>(irq13), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(46, reinterpret_cast<std::uint64_t>(irq14), GateType::InterruptGate, PrivilegeLevel::Kernel);
            set_idt_entry(47, reinterpret_cast<std::uint64_t>(irq15), GateType::InterruptGate, PrivilegeLevel::Kernel);
        }
    }
    
    // Core interrupt management functions
    
    /// Initialize interrupt handling subsystem
    [[nodiscard]] InterruptResult<void> init() noexcept {
        if (Internal::initialized.load()) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        try {
            // Clear IDT
            Internal::idt_table.fill(IDTEntry{});
            Internal::handlers.fill(nullptr);
            
            // Set up IDT pointer
            Internal::idt_ptr.limit = sizeof(Internal::idt_table) - 1;
            Internal::idt_ptr.base = reinterpret_cast<std::uint64_t>(Internal::idt_table.data());
            
            // Set up exception handlers
            Internal::setup_exception_isrs();
            
            // Set up IRQ handlers  
            Internal::setup_irq_handlers();
            
            // Initialize interrupt controller (try APIC first, fallback to PIC)
            Internal::controller = std::make_unique<Controllers::APIC>(true);
            auto controller_result = Internal::controller->initialize();
            
            if (!controller_result.has_value()) {
                // Fallback to legacy PIC
                Internal::controller = std::make_unique<Controllers::PIC>();
                controller_result = Internal::controller->initialize();
                
                if (!controller_result.has_value()) {
                    return compat::unexpected(controller_result.error());
                }
            }
            
            // Register default exception handlers
            register_default_exception_handlers();
            
            // Load IDT
            LowLevel::load_idt(Internal::idt_ptr);
            
            // Clear statistics
            Internal::stats = InterruptStatistics{};
            
            Internal::initialized = true;
            
        } catch (...) {
            return compat::unexpected(InterruptError::ResourceLimit);
        }
        
        return {};
    }
    
    // Template specialization for registering handlers
    template<InterruptHandler Handler>
    [[nodiscard]] InterruptResult<void> register_handler(
        std::uint8_t vector,
        Handler&& handler,
        InterruptType type,
        PrivilegeLevel privilege
    ) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        if (!is_valid_vector(vector)) {
            return compat::unexpected(InterruptError::InvalidVector);
        }
        
        try {
            std::lock_guard lock(Internal::handler_mutex);
            
            // Check if already registered
            if (Internal::handlers[vector]) {
                return compat::unexpected(InterruptError::AlreadyRegistered);
            }
            
            // Store handler
            Internal::handlers[vector] = std::forward<Handler>(handler);
            
        } catch (...) {
            return compat::unexpected(InterruptError::ResourceLimit);
        }
        
        return {};
    }
    
    // Explicit instantiations for common handler types
    template InterruptResult<void> register_handler(
        std::uint8_t vector,
        std::function<void(InterruptFrame&)>&& handler,
        InterruptType type,
        PrivilegeLevel privilege
    ) noexcept;
    
    template InterruptResult<void> register_handler(
        std::uint8_t vector,
        void(*handler)(InterruptFrame&),
        InterruptType type,
        PrivilegeLevel privilege
    ) noexcept;
    
    /// Unregister an interrupt handler
    [[nodiscard]] InterruptResult<void> unregister_handler(std::uint8_t vector) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        if (!is_valid_vector(vector)) {
            return compat::unexpected(InterruptError::InvalidVector);
        }
        
        std::lock_guard lock(Internal::handler_mutex);
        Internal::handlers[vector] = nullptr;
        
        return {};
    }
    
    /// Enable interrupts globally
    [[nodiscard]] InterruptResult<void> enable() noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        LowLevel::sti();
        return {};
    }
    
    /// Disable interrupts globally
    [[nodiscard]] InterruptResult<void> disable() noexcept {
        LowLevel::cli();
        return {};
    }
    
    /// Check if interrupts are enabled
    [[nodiscard]] bool are_enabled() noexcept {
        auto flags = LowLevel::read_flags();
        return (flags & 0x200) != 0; // IF flag
    }
    
    /// Enable specific IRQ line
    [[nodiscard]] InterruptResult<void> enable_irq(std::uint8_t irq) noexcept {
        if (!Internal::controller) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        return Internal::controller->enable_irq(irq);
    }
    
    /// Disable specific IRQ line
    [[nodiscard]] InterruptResult<void> disable_irq(std::uint8_t irq) noexcept {
        if (!Internal::controller) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        return Internal::controller->disable_irq(irq);
    }
    
    /// Send End of Interrupt signal
    [[nodiscard]] InterruptResult<void> send_eoi(std::uint8_t irq) noexcept {
        if (!Internal::controller) {
            return compat::unexpected(InterruptError::InvalidState);
        }
        
        return Internal::controller->send_eoi(irq);
    }
    
    /// Get interrupt statistics
    [[nodiscard]] const InterruptStatistics& get_statistics() noexcept {
        return Internal::stats;
    }
    
    /// Get exception name
    [[nodiscard]] std::string_view get_exception_name(std::uint8_t vector) noexcept {
        constexpr std::array<std::string_view, 32> exception_names = {
            "Divide Error", "Debug", "Non-Maskable Interrupt", "Breakpoint",
            "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
            "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
            "Stack Fault", "General Protection Fault", "Page Fault", "Reserved",
            "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD Exception",
            "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
            "Reserved", "Reserved", "Reserved", "Reserved",
            "Reserved", "Reserved", "Reserved", "Reserved"
        };
        
        return vector < exception_names.size() ? exception_names[vector] : "Unknown";
    }
    
    /// Register default exception handlers
    void register_default_exception_handlers() noexcept {
        // Register standard exception handlers
        Internal::exception_handlers[DIVIDE_ERROR] = 
            std::make_unique<ExceptionHandler>(DIVIDE_ERROR, "Divide Error", Exceptions::handle_divide_error);
        Internal::exception_handlers[DEBUG] = 
            std::make_unique<ExceptionHandler>(DEBUG, "Debug", Exceptions::handle_debug);
        Internal::exception_handlers[NMI] = 
            std::make_unique<ExceptionHandler>(NMI, "NMI", Exceptions::handle_nmi);
        Internal::exception_handlers[BREAKPOINT] = 
            std::make_unique<ExceptionHandler>(BREAKPOINT, "Breakpoint", Exceptions::handle_breakpoint);
        Internal::exception_handlers[PAGE_FAULT] = 
            std::make_unique<ExceptionHandler>(PAGE_FAULT, "Page Fault", Exceptions::handle_page_fault);
        Internal::exception_handlers[GENERAL_PROTECTION] = 
            std::make_unique<ExceptionHandler>(GENERAL_PROTECTION, "General Protection Fault", Exceptions::handle_general_protection);
        
        // Register default hardware interrupt handlers
        register_handler(IRQ_TIMER, Hardware::handle_timer, InterruptType::Hardware, PrivilegeLevel::Kernel);
        register_handler(IRQ_KEYBOARD, Hardware::handle_keyboard, InterruptType::Hardware, PrivilegeLevel::Kernel);
    }
    
    /// Common exception handlers
    namespace Exceptions {
        void handle_divide_error(InterruptFrame& frame) noexcept {
            // Handle division by zero
            // TODO: Signal process or terminate
        }
        
        void handle_debug(InterruptFrame& frame) noexcept {
            // Handle debug exception
            // TODO: Implement debugger support
        }
        
        void handle_nmi(InterruptFrame& frame) noexcept {
            // Handle non-maskable interrupt
            // TODO: System health checks
        }
        
        void handle_breakpoint(InterruptFrame& frame) noexcept {
            // Handle breakpoint
            // TODO: Debugger integration
        }
        
        void handle_page_fault(InterruptFrame& frame) noexcept {
            // Handle page fault with detailed analysis
            auto fault_address = LowLevel::read_cr2();
            auto error_code = static_cast<PageFaultError>(frame.error_code);
            
            // TODO: Implement proper page fault handling
            // - Check if address is valid
            // - Handle copy-on-write
            // - Allocate new pages
            // - Signal segmentation fault if invalid
        }
        
        void handle_general_protection(InterruptFrame& frame) noexcept {
            // Handle general protection fault
            // TODO: Analyze cause and handle appropriately
        }
        
        void handle_generic_exception(InterruptFrame& frame) noexcept {
            // Generic exception handler for unhandled exceptions
            // TODO: Log exception details and terminate process
        }
    }
    
    /// Hardware interrupt handlers  
    namespace Hardware {
        void handle_timer(InterruptFrame& frame) noexcept {
            // Handle timer interrupt for scheduling
            // TODO: Update system time
            // TODO: Trigger process scheduler
            Process::schedule();
        }
        
        void handle_keyboard(InterruptFrame& frame) noexcept {
            // Handle keyboard input
            // TODO: Read scancode and process input
            std::uint8_t scancode = 0; // TODO: Read from port 0x60
        }
        
        void handle_spurious(InterruptFrame& frame) noexcept {
            Internal::stats.spurious_interrupts.fetch_add(1);
            // Don't send EOI for spurious interrupts
        }
    }
    
    /// Low-level interrupt control
    namespace LowLevel {
        void load_idt(const IDTPointer& idt_ptr) noexcept {
            asm volatile("lidt %0" : : "m"(idt_ptr));
        }
        
        [[nodiscard]] IDTPointer store_idt() noexcept {
            IDTPointer idt_ptr;
            asm volatile("sidt %0" : "=m"(idt_ptr));
            return idt_ptr;
        }
        
        void sti() noexcept {
            asm volatile("sti");
        }
        
        void cli() noexcept {
            asm volatile("cli");
        }
        
        void hlt() noexcept {
            asm volatile("hlt");
        }
        
        void nop() noexcept {
            asm volatile("nop");
        }
        
        void mfence() noexcept {
            asm volatile("mfence" ::: "memory");
        }
        
        [[nodiscard]] std::uint64_t read_flags() noexcept {
            std::uint64_t flags;
            asm volatile("pushfq; popq %0" : "=r"(flags));
            return flags;
        }
        
        void write_flags(std::uint64_t flags) noexcept {
            asm volatile("pushq %0; popfq" : : "r"(flags) : "memory");
        }
        
        [[nodiscard]] std::uint64_t read_cr2() noexcept {
            std::uint64_t value;
            asm volatile("movq %%cr2, %0" : "=r"(value));
            return value;
        }
        
        [[nodiscard]] std::uint64_t read_cr3() noexcept {
            std::uint64_t value;
            asm volatile("movq %%cr3, %0" : "=r"(value));
            return value;
        }
        
        void write_cr3(std::uint64_t value) noexcept {
            asm volatile("movq %0, %%cr3" : : "r"(value) : "memory");
        }
    }
    
    /// Interrupt controller implementations
    namespace Controllers {
        // Legacy PIC implementation
        InterruptResult<void> PIC::initialize() noexcept {
            // Remap PIC to avoid conflicts with CPU exceptions
            send_command(PIC1_COMMAND, 0x11); // Initialize PIC1
            send_command(PIC2_COMMAND, 0x11); // Initialize PIC2
            
            send_data(PIC1_DATA, 0x20); // PIC1 vector offset (32)
            send_data(PIC2_DATA, 0x28); // PIC2 vector offset (40)
            
            send_data(PIC1_DATA, 0x04); // PIC1 has slave at IRQ2
            send_data(PIC2_DATA, 0x02); // PIC2 cascade identity
            
            send_data(PIC1_DATA, 0x01); // 8086 mode
            send_data(PIC2_DATA, 0x01); // 8086 mode
            
            // Mask all interrupts initially
            send_data(PIC1_DATA, 0xFF);
            send_data(PIC2_DATA, 0xFF);
            
            return {};
        }
        
        InterruptResult<void> PIC::enable_irq(std::uint8_t irq) noexcept {
            if (irq >= 16) {
                return compat::unexpected(InterruptError::InvalidVector);
            }
            
            irq_mask_.reset(irq);
            
            std::uint8_t mask;
            if (irq < 8) {
                mask = static_cast<std::uint8_t>(irq_mask_.to_ulong() & 0xFF);
                send_data(PIC1_DATA, mask);
            } else {
                mask = static_cast<std::uint8_t>((irq_mask_.to_ulong() >> 8) & 0xFF);
                send_data(PIC2_DATA, mask);
            }
            
            return {};
        }
        
        InterruptResult<void> PIC::send_eoi(std::uint8_t irq) noexcept {
            if (irq >= 8) {
                send_command(PIC2_COMMAND, 0x20);
            }
            send_command(PIC1_COMMAND, 0x20);
            
            return {};
        }
        
        void PIC::send_command(std::uint16_t port, std::uint8_t command) noexcept {
            asm volatile("outb %0, %1" : : "a"(command), "Nd"(port));
        }
        
        void PIC::send_data(std::uint16_t port, std::uint8_t data) noexcept {
            asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
        }
        
        [[nodiscard]] std::uint8_t PIC::read_data(std::uint16_t port) noexcept {
            std::uint8_t value;
            asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
            return value;
        }
    }
    
} // namespace Interrupt