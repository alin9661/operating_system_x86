#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <expected>
#include <concepts>
#include <functional>
#include <array>
#include <atomic>
#include <chrono>
#include <string_view>
#include <bitset>
#include <span>
#include <optional>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace Interrupt {
    // Modern interrupt constants for x86_64
    inline constexpr std::size_t IDT_ENTRIES = 256;
    inline constexpr std::size_t ISR_STACK_SIZE = 8192; // 8KB
    inline constexpr std::uint8_t USER_INTERRUPT_BASE = 32;
    inline constexpr std::uint8_t MAX_IRQ_LINES = 24;
    
    // Interrupt vector numbers (x86_64)
    inline constexpr std::uint8_t DIVIDE_ERROR = 0;
    inline constexpr std::uint8_t DEBUG = 1;
    inline constexpr std::uint8_t NMI = 2;
    inline constexpr std::uint8_t BREAKPOINT = 3;
    inline constexpr std::uint8_t OVERFLOW = 4;
    inline constexpr std::uint8_t BOUND_RANGE = 5;
    inline constexpr std::uint8_t INVALID_OPCODE = 6;
    inline constexpr std::uint8_t DEVICE_NOT_AVAILABLE = 7;
    inline constexpr std::uint8_t DOUBLE_FAULT = 8;
    inline constexpr std::uint8_t INVALID_TSS = 10;
    inline constexpr std::uint8_t SEGMENT_NOT_PRESENT = 11;
    inline constexpr std::uint8_t STACK_FAULT = 12;
    inline constexpr std::uint8_t GENERAL_PROTECTION = 13;
    inline constexpr std::uint8_t PAGE_FAULT = 14;
    inline constexpr std::uint8_t X87_FPU = 16;
    inline constexpr std::uint8_t ALIGNMENT_CHECK = 17;
    inline constexpr std::uint8_t MACHINE_CHECK = 18;
    inline constexpr std::uint8_t SIMD_EXCEPTION = 19;
    inline constexpr std::uint8_t VIRTUALIZATION = 20;
    inline constexpr std::uint8_t CONTROL_PROTECTION = 21;
    
    // Hardware IRQ numbers
    inline constexpr std::uint8_t IRQ_TIMER = 32;
    inline constexpr std::uint8_t IRQ_KEYBOARD = 33;
    inline constexpr std::uint8_t IRQ_CASCADE = 34;
    inline constexpr std::uint8_t IRQ_COM2 = 35;
    inline constexpr std::uint8_t IRQ_COM1 = 36;
    inline constexpr std::uint8_t IRQ_LPT2 = 37;
    inline constexpr std::uint8_t IRQ_FLOPPY = 38;
    inline constexpr std::uint8_t IRQ_LPT1 = 39;
    inline constexpr std::uint8_t IRQ_RTC = 40;
    inline constexpr std::uint8_t IRQ_FREE1 = 41;
    inline constexpr std::uint8_t IRQ_FREE2 = 42;
    inline constexpr std::uint8_t IRQ_FREE3 = 43;
    inline constexpr std::uint8_t IRQ_MOUSE = 44;
    inline constexpr std::uint8_t IRQ_FPU = 45;
    inline constexpr std::uint8_t IRQ_PIDE = 46;
    inline constexpr std::uint8_t IRQ_SIDE = 47;
    
    // Interrupt error types
    enum class InterruptError : std::uint8_t {
        Success = 0,
        InvalidVector,
        HandlerNotFound,
        AlreadyRegistered,
        PermissionDenied,
        InvalidState,
        ResourceLimit,
        HardwareFault,
        StackOverflow,
        DeadlockDetected,
        TimeoutExpired
    };
    
    // Interrupt types
    enum class InterruptType : std::uint8_t {
        Exception = 0,      // CPU exceptions (divide by zero, page fault, etc.)
        Hardware = 1,       // Hardware IRQs (timer, keyboard, etc.)
        Software = 2,       // Software interrupts (system calls)
        NMI = 3,           // Non-maskable interrupts
        IPI = 4            // Inter-processor interrupts (SMP)
    };
    
    // Interrupt privilege levels
    enum class PrivilegeLevel : std::uint8_t {
        Kernel = 0,         // Ring 0 - kernel mode only
        Reserved1 = 1,      // Ring 1 - typically unused
        Reserved2 = 2,      // Ring 2 - typically unused
        User = 3            // Ring 3 - user mode
    };
    
    // Gate types for IDT entries
    enum class GateType : std::uint8_t {
        TaskGate = 0x5,
        InterruptGate = 0xE,
        TrapGate = 0xF
    };
    
    // Interrupt descriptor flags
    enum class InterruptFlags : std::uint16_t {
        Present = 1 << 15,
        DPL0 = 0 << 13,     // Descriptor Privilege Level 0
        DPL1 = 1 << 13,     // Descriptor Privilege Level 1
        DPL2 = 2 << 13,     // Descriptor Privilege Level 2
        DPL3 = 3 << 13,     // Descriptor Privilege Level 3
        StorageSegment = 1 << 12,
        InterruptGate = 0xE,
        TrapGate = 0xF,
        TaskGate = 0x5
    };
    
    // Enable bitwise operations on InterruptFlags
    constexpr InterruptFlags operator|(InterruptFlags a, InterruptFlags b) noexcept {
        return static_cast<InterruptFlags>(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
    }
    
    constexpr InterruptFlags operator&(InterruptFlags a, InterruptFlags b) noexcept {
        return static_cast<InterruptFlags>(static_cast<std::uint16_t>(a) & static_cast<std::uint16_t>(b));
    }
    
    constexpr InterruptFlags operator~(InterruptFlags a) noexcept {
        return static_cast<InterruptFlags>(~static_cast<std::uint16_t>(a));
    }
    
    // Page fault error code flags
    enum class PageFaultError : std::uint32_t {
        Present = 1 << 0,           // Page not present
        Write = 1 << 1,             // Write access
        User = 1 << 2,              // User mode access
        ReservedWrite = 1 << 3,     // Reserved bit violation
        InstructionFetch = 1 << 4,  // Instruction fetch
        ProtectionKey = 1 << 5,     // Protection key violation
        ShadowStack = 1 << 6,       // Shadow stack access
        SGX = 1 << 15               // SGX violation
    };
    
    // CPU interrupt frame for x86_64
    struct alignas(16) InterruptFrame {
        // Saved by hardware
        std::uint64_t rip;
        std::uint64_t cs;
        std::uint64_t rflags;
        std::uint64_t rsp;
        std::uint64_t ss;
        
        // Error code (if applicable)
        std::uint64_t error_code;
        
        // Saved by interrupt stub
        std::uint64_t rax, rbx, rcx, rdx;
        std::uint64_t rsi, rdi, rbp;
        std::uint64_t r8, r9, r10, r11;
        std::uint64_t r12, r13, r14, r15;
        
        // Segment registers
        std::uint16_t ds, es, fs, gs;
        
        // Vector number
        std::uint8_t vector;
        
        // Default initialization
        InterruptFrame() noexcept = default;
        
        // Utility functions
        [[nodiscard]] bool from_user_mode() const noexcept {
            return (cs & 3) == 3;
        }
        
        [[nodiscard]] bool has_error_code() const noexcept {
            return vector == DOUBLE_FAULT || vector == INVALID_TSS ||
                   vector == SEGMENT_NOT_PRESENT || vector == STACK_FAULT ||
                   vector == GENERAL_PROTECTION || vector == PAGE_FAULT ||
                   vector == ALIGNMENT_CHECK;
        }
    };
    
    // Interrupt statistics
    struct InterruptStatistics {
        std::atomic<std::uint64_t> total_interrupts{0};
        std::atomic<std::uint64_t> hardware_interrupts{0};
        std::atomic<std::uint64_t> software_interrupts{0};
        std::atomic<std::uint64_t> exceptions{0};
        std::atomic<std::uint64_t> spurious_interrupts{0};
        std::atomic<std::uint64_t> nested_interrupts{0};
        std::atomic<std::uint64_t> lost_interrupts{0};
        std::array<std::atomic<std::uint64_t>, IDT_ENTRIES> vector_counts{};
        
        // Timing statistics
        std::chrono::nanoseconds max_handler_time{0};
        std::chrono::nanoseconds avg_handler_time{0};
        std::chrono::nanoseconds total_handler_time{0};
        
        [[nodiscard]] double interrupt_rate() const noexcept {
            return static_cast<double>(total_interrupts.load());
        }
        
        [[nodiscard]] std::uint64_t get_vector_count(std::uint8_t vector) const noexcept {
            return vector < IDT_ENTRIES ? vector_counts[vector].load() : 0;
        }
    };
    
    // Forward declarations
    class InterruptController;
    class InterruptManager;
    class ExceptionHandler;
    
    // Modern interrupt handler concept
    template<typename T>
    concept InterruptHandler = requires(T handler, InterruptFrame& frame) {
        { handler(frame) } -> std::same_as<void>;
    } || requires(T handler, InterruptFrame& frame, std::uint8_t vector) {
        { handler(frame, vector) } -> std::same_as<void>;
    };
    
    // Interrupt result type
    template<typename T>
    using InterruptResult = std::expected<T, InterruptError>;
    
    // RAII interrupt state guard
    class InterruptGuard {
    private:
        bool was_enabled_;
        
    public:
        InterruptGuard() noexcept : was_enabled_(are_enabled()) {
            disable();
        }
        
        ~InterruptGuard() noexcept {
            if (was_enabled_) {
                enable();
            }
        }
        
        // Non-copyable, non-movable
        InterruptGuard(const InterruptGuard&) = delete;
        InterruptGuard& operator=(const InterruptGuard&) = delete;
        InterruptGuard(InterruptGuard&&) = delete;
        InterruptGuard& operator=(InterruptGuard&&) = delete;
        
        [[nodiscard]] bool was_enabled() const noexcept { return was_enabled_; }
    };
    
    // RAII critical section guard
    class CriticalSection {
    private:
        std::unique_lock<std::mutex> lock_;
        InterruptGuard interrupt_guard_;
        
    public:
        explicit CriticalSection(std::mutex& mutex) noexcept
            : lock_(mutex), interrupt_guard_() {}
        
        ~CriticalSection() noexcept = default;
        
        // Non-copyable, movable
        CriticalSection(const CriticalSection&) = delete;
        CriticalSection& operator=(const CriticalSection&) = delete;
        CriticalSection(CriticalSection&&) = default;
        CriticalSection& operator=(CriticalSection&&) = default;
    };
    
    // Interrupt descriptor table entry
    struct alignas(16) IDTEntry {
        std::uint16_t offset_low;    // Offset bits 0-15
        std::uint16_t selector;      // Code segment selector
        std::uint8_t ist;           // Interrupt Stack Table offset
        std::uint8_t type_attr;     // Type and attributes
        std::uint16_t offset_mid;    // Offset bits 16-31
        std::uint32_t offset_high;   // Offset bits 32-63
        std::uint32_t reserved;      // Reserved, must be zero
        
        // Constructor
        IDTEntry() noexcept = default;
        
        // Set interrupt gate
        void set_gate(std::uint64_t handler_addr, std::uint16_t code_selector, 
                     GateType type, PrivilegeLevel dpl, bool present = true) noexcept {
            offset_low = static_cast<std::uint16_t>(handler_addr & 0xFFFF);
            offset_mid = static_cast<std::uint16_t>((handler_addr >> 16) & 0xFFFF);
            offset_high = static_cast<std::uint32_t>((handler_addr >> 32) & 0xFFFFFFFF);
            selector = code_selector;
            ist = 0; // No IST by default
            type_attr = static_cast<std::uint8_t>(type) | 
                       (static_cast<std::uint8_t>(dpl) << 5) |
                       (present ? 0x80 : 0x00);
            reserved = 0;
        }
        
        [[nodiscard]] std::uint64_t get_handler_address() const noexcept {
            return static_cast<std::uint64_t>(offset_low) |
                   (static_cast<std::uint64_t>(offset_mid) << 16) |
                   (static_cast<std::uint64_t>(offset_high) << 32);
        }
    };
    
    // IDT pointer structure
    struct alignas(16) IDTPointer {
        std::uint16_t limit;
        std::uint64_t base;
    } __attribute__((packed));
    
    // Exception handler class
    class ExceptionHandler {
    private:
        std::uint8_t vector_;
        std::string_view name_;
        std::function<void(InterruptFrame&)> handler_;
        std::atomic<std::uint64_t> count_{0};
        std::atomic<bool> enabled_{true};
        
    public:
        ExceptionHandler(std::uint8_t vector, std::string_view name,
                        std::function<void(InterruptFrame&)> handler) noexcept
            : vector_(vector), name_(name), handler_(std::move(handler)) {}
        
        void handle(InterruptFrame& frame) noexcept {
            if (enabled_.load() && handler_) {
                count_.fetch_add(1);
                handler_(frame);
            }
        }
        
        [[nodiscard]] std::uint8_t vector() const noexcept { return vector_; }
        [[nodiscard]] std::string_view name() const noexcept { return name_; }
        [[nodiscard]] std::uint64_t count() const noexcept { return count_.load(); }
        [[nodiscard]] bool enabled() const noexcept { return enabled_.load(); }
        
        void enable() noexcept { enabled_.store(true); }
        void disable() noexcept { enabled_.store(false); }
    };
    
    // Interrupt controller interface
    class InterruptController {
    public:
        virtual ~InterruptController() = default;
        
        virtual InterruptResult<void> initialize() noexcept = 0;
        virtual InterruptResult<void> enable_irq(std::uint8_t irq) noexcept = 0;
        virtual InterruptResult<void> disable_irq(std::uint8_t irq) noexcept = 0;
        virtual InterruptResult<void> mask_irq(std::uint8_t irq) noexcept = 0;
        virtual InterruptResult<void> unmask_irq(std::uint8_t irq) noexcept = 0;
        virtual InterruptResult<void> send_eoi(std::uint8_t irq) noexcept = 0;
        virtual InterruptResult<std::uint8_t> get_irq_vector(std::uint8_t irq) noexcept = 0;
        virtual InterruptResult<bool> is_spurious(std::uint8_t vector) noexcept = 0;
    };
    
    // Modern interrupt management functions with C++20 features
    
    /// Initialize interrupt handling subsystem
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> init() noexcept;
    
    /// Register an interrupt handler
    /// @param vector Interrupt vector number
    /// @param handler Handler function
    /// @param type Interrupt type
    /// @param privilege Privilege level required
    /// @return Success or error code
    template<InterruptHandler Handler>
    [[nodiscard]] InterruptResult<void> register_handler(
        std::uint8_t vector,
        Handler&& handler,
        InterruptType type = InterruptType::Hardware,
        PrivilegeLevel privilege = PrivilegeLevel::Kernel
    ) noexcept;
    
    /// Unregister an interrupt handler
    /// @param vector Interrupt vector number
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> unregister_handler(std::uint8_t vector) noexcept;
    
    /// Enable interrupts globally
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> enable() noexcept;
    
    /// Disable interrupts globally
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> disable() noexcept;
    
    /// Check if interrupts are enabled
    /// @return true if interrupts are enabled
    [[nodiscard]] bool are_enabled() noexcept;
    
    /// Enable specific IRQ line
    /// @param irq IRQ number
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> enable_irq(std::uint8_t irq) noexcept;
    
    /// Disable specific IRQ line
    /// @param irq IRQ number
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> disable_irq(std::uint8_t irq) noexcept;
    
    /// Send End of Interrupt signal
    /// @param irq IRQ number
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> send_eoi(std::uint8_t irq) noexcept;
    
    /// Get interrupt statistics
    /// @return Current interrupt statistics
    [[nodiscard]] const InterruptStatistics& get_statistics() noexcept;
    
    /// Check if vector is valid
    /// @param vector Interrupt vector
    /// @return true if vector is valid
    [[nodiscard]] constexpr bool is_valid_vector(std::uint8_t vector) noexcept {
        return vector < IDT_ENTRIES;
    }
    
    /// Check if vector is exception
    /// @param vector Interrupt vector
    /// @return true if vector is exception
    [[nodiscard]] constexpr bool is_exception(std::uint8_t vector) noexcept {
        return vector < USER_INTERRUPT_BASE;
    }
    
    /// Check if vector is hardware IRQ
    /// @param vector Interrupt vector
    /// @return true if vector is hardware IRQ
    [[nodiscard]] constexpr bool is_hardware_irq(std::uint8_t vector) noexcept {
        return vector >= USER_INTERRUPT_BASE && vector < (USER_INTERRUPT_BASE + MAX_IRQ_LINES);
    }
    
    /// Get exception name
    /// @param vector Exception vector
    /// @return Exception name or empty string
    [[nodiscard]] std::string_view get_exception_name(std::uint8_t vector) noexcept;
    
    /// Trigger software interrupt
    /// @param vector Interrupt vector
    /// @return Success or error code
    [[nodiscard]] InterruptResult<void> trigger_interrupt(std::uint8_t vector) noexcept;
    
    /// Wait for interrupt
    /// @param timeout Maximum time to wait
    /// @return Success or timeout error
    [[nodiscard]] InterruptResult<void> wait_for_interrupt(
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max()
    ) noexcept;
    
    /// Common exception handlers with modern C++ patterns
    namespace Exceptions {
        /// Handle divide by zero exception
        void handle_divide_error(InterruptFrame& frame) noexcept;
        
        /// Handle debug exception
        void handle_debug(InterruptFrame& frame) noexcept;
        
        /// Handle non-maskable interrupt
        void handle_nmi(InterruptFrame& frame) noexcept;
        
        /// Handle breakpoint exception
        void handle_breakpoint(InterruptFrame& frame) noexcept;
        
        /// Handle overflow exception
        void handle_overflow(InterruptFrame& frame) noexcept;
        
        /// Handle bound range exceeded
        void handle_bound_range(InterruptFrame& frame) noexcept;
        
        /// Handle invalid opcode
        void handle_invalid_opcode(InterruptFrame& frame) noexcept;
        
        /// Handle device not available
        void handle_device_not_available(InterruptFrame& frame) noexcept;
        
        /// Handle double fault
        void handle_double_fault(InterruptFrame& frame) noexcept;
        
        /// Handle invalid TSS
        void handle_invalid_tss(InterruptFrame& frame) noexcept;
        
        /// Handle segment not present
        void handle_segment_not_present(InterruptFrame& frame) noexcept;
        
        /// Handle stack fault
        void handle_stack_fault(InterruptFrame& frame) noexcept;
        
        /// Handle general protection fault
        void handle_general_protection(InterruptFrame& frame) noexcept;
        
        /// Handle page fault with detailed analysis
        void handle_page_fault(InterruptFrame& frame) noexcept;
        
        /// Handle x87 FPU error
        void handle_x87_fpu(InterruptFrame& frame) noexcept;
        
        /// Handle alignment check
        void handle_alignment_check(InterruptFrame& frame) noexcept;
        
        /// Handle machine check
        void handle_machine_check(InterruptFrame& frame) noexcept;
        
        /// Handle SIMD exception
        void handle_simd_exception(InterruptFrame& frame) noexcept;
        
        /// Handle virtualization exception
        void handle_virtualization(InterruptFrame& frame) noexcept;
        
        /// Handle control protection exception
        void handle_control_protection(InterruptFrame& frame) noexcept;
    }
    
    /// Hardware interrupt handlers
    namespace Hardware {
        /// Handle timer interrupt
        void handle_timer(InterruptFrame& frame) noexcept;
        
        /// Handle keyboard interrupt
        void handle_keyboard(InterruptFrame& frame) noexcept;
        
        /// Handle mouse interrupt
        void handle_mouse(InterruptFrame& frame) noexcept;
        
        /// Handle real-time clock
        void handle_rtc(InterruptFrame& frame) noexcept;
        
        /// Handle serial port interrupts
        void handle_serial(InterruptFrame& frame) noexcept;
        
        /// Handle parallel port interrupts
        void handle_parallel(InterruptFrame& frame) noexcept;
        
        /// Handle disk interrupts
        void handle_disk(InterruptFrame& frame) noexcept;
        
        /// Handle network interrupts
        void handle_network(InterruptFrame& frame) noexcept;
        
        /// Handle spurious interrupts
        void handle_spurious(InterruptFrame& frame) noexcept;
    }
    
    /// Low-level interrupt control
    namespace LowLevel {
        /// Load IDT
        /// @param idt_ptr Pointer to IDT descriptor
        void load_idt(const IDTPointer& idt_ptr) noexcept;
        
        /// Store IDT
        /// @return Current IDT descriptor
        [[nodiscard]] IDTPointer store_idt() noexcept;
        
        /// Enable interrupts (STI)
        void sti() noexcept;
        
        /// Disable interrupts (CLI)
        void cli() noexcept;
        
        /// Halt and wait for interrupt
        void hlt() noexcept;
        
        /// No operation
        void nop() noexcept;
        
        /// Memory fence
        void mfence() noexcept;
        
        /// Read flags register
        [[nodiscard]] std::uint64_t read_flags() noexcept;
        
        /// Write flags register
        void write_flags(std::uint64_t flags) noexcept;
        
        /// Read control register 2 (page fault address)
        [[nodiscard]] std::uint64_t read_cr2() noexcept;
        
        /// Read control register 3 (page directory)
        [[nodiscard]] std::uint64_t read_cr3() noexcept;
        
        /// Write control register 3 (page directory)
        void write_cr3(std::uint64_t value) noexcept;
    }
    
    /// Interrupt controller implementations
    namespace Controllers {
        /// Legacy PIC (8259) controller
        class PIC : public InterruptController {
        private:
            static constexpr std::uint16_t PIC1_COMMAND = 0x20;
            static constexpr std::uint16_t PIC1_DATA = 0x21;
            static constexpr std::uint16_t PIC2_COMMAND = 0xA0;
            static constexpr std::uint16_t PIC2_DATA = 0xA1;
            
            std::bitset<16> irq_mask_{0xFFFF}; // All IRQs masked initially
            
        public:
            InterruptResult<void> initialize() noexcept override;
            InterruptResult<void> enable_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> disable_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> mask_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> unmask_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> send_eoi(std::uint8_t irq) noexcept override;
            InterruptResult<std::uint8_t> get_irq_vector(std::uint8_t irq) noexcept override;
            InterruptResult<bool> is_spurious(std::uint8_t vector) noexcept override;
            
        private:
            void send_command(std::uint16_t port, std::uint8_t command) noexcept;
            void send_data(std::uint16_t port, std::uint8_t data) noexcept;
            [[nodiscard]] std::uint8_t read_data(std::uint16_t port) noexcept;
        };
        
        /// Advanced PIC (APIC/x2APIC) controller
        class APIC : public InterruptController {
        private:
            std::uint64_t apic_base_;
            bool x2apic_mode_;
            std::array<std::atomic<bool>, 256> irq_enabled_{};
            
        public:
            explicit APIC(bool enable_x2apic = true) noexcept;
            
            InterruptResult<void> initialize() noexcept override;
            InterruptResult<void> enable_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> disable_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> mask_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> unmask_irq(std::uint8_t irq) noexcept override;
            InterruptResult<void> send_eoi(std::uint8_t irq) noexcept override;
            InterruptResult<std::uint8_t> get_irq_vector(std::uint8_t irq) noexcept override;
            InterruptResult<bool> is_spurious(std::uint8_t vector) noexcept override;
            
            // APIC-specific functions
            [[nodiscard]] InterruptResult<void> send_ipi(std::uint32_t target_cpu, std::uint8_t vector) noexcept;
            [[nodiscard]] InterruptResult<std::uint32_t> get_local_apic_id() noexcept;
            [[nodiscard]] InterruptResult<void> calibrate_timer() noexcept;
            
        private:
            [[nodiscard]] std::uint32_t read_apic_register(std::uint32_t offset) noexcept;
            void write_apic_register(std::uint32_t offset, std::uint32_t value) noexcept;
        };
    }
    
    /// Performance monitoring
    namespace Performance {
        /// Interrupt latency measurement
        struct LatencyMeasurement {
            std::chrono::high_resolution_clock::time_point start_time;
            std::chrono::high_resolution_clock::time_point end_time;
            std::uint8_t vector;
            
            [[nodiscard]] std::chrono::nanoseconds latency() const noexcept {
                return std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
            }
        };
        
        /// Start latency measurement
        void start_measurement(std::uint8_t vector) noexcept;
        
        /// End latency measurement
        void end_measurement(std::uint8_t vector) noexcept;
        
        /// Get average latency for vector
        [[nodiscard]] std::chrono::nanoseconds get_average_latency(std::uint8_t vector) noexcept;
        
        /// Get maximum latency for vector
        [[nodiscard]] std::chrono::nanoseconds get_max_latency(std::uint8_t vector) noexcept;
        
        /// Reset measurements
        void reset_measurements() noexcept;
    }
    
    /// Debug and introspection functions
    #ifdef DEBUG
    void dump_idt() noexcept;
    void dump_interrupt_statistics() noexcept;
    void dump_interrupt_handlers() noexcept;
    void validate_idt() noexcept;
    void test_interrupt_latency() noexcept;
    #endif
    
} // namespace Interrupt