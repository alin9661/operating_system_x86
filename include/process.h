#pragma once

#include "compat/cstdint"
#include "compat/cstddef"
#include "compat/memory"
#include "compat/expected"
#include "compat/concepts"
#include <string_view>
#include "compat/span"
#include "compat/array"
#include "compat/atomic"
#include <chrono>
#include <functional>
#include <optional>
#include <variant>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <coroutine>
#include "memory.h"

namespace Process {
    // Process ID type
    using ProcessId = std::uint32_t;
    using ThreadId = std::uint32_t;
    using UserId = std::uint32_t;
    using GroupId = std::uint32_t;
    
    // Special process IDs
    inline constexpr ProcessId INVALID_PID = 0;
    inline constexpr ProcessId KERNEL_PID = 1;
    inline constexpr ProcessId INIT_PID = 2;
    
    // Process limits
    inline constexpr std::size_t MAX_PROCESSES = 65536;
    inline constexpr std::size_t MAX_THREADS_PER_PROCESS = 1024;
    inline constexpr std::size_t MAX_OPEN_FILES = 1024;
    inline constexpr std::size_t DEFAULT_STACK_SIZE = 8 * 1024 * 1024; // 8MB
    inline constexpr std::size_t DEFAULT_HEAP_SIZE = 64 * 1024 * 1024; // 64MB
    
    // Process error types
    enum class ProcessError : std::uint8_t {
        Success = 0,
        ProcessNotFound,
        InvalidPid,
        OutOfMemory,
        PermissionDenied,
        InvalidState,
        ResourceLimit,
        InvalidArgument,
        DeadlockDetected,
        TimeoutExpired,
        OperationCancelled,
        SystemCall
    };
    
    // Process states with modern enum features
    enum class ProcessState : std::uint8_t {
        Created = 0,     // Process created but not yet scheduled
        Ready = 1,       // Ready to run
        Running = 2,     // Currently executing
        Blocked = 3,     // Waiting for resource
        Suspended = 4,   // Suspended by user/system
        Zombie = 5,      // Terminated but not reaped
        Terminated = 6   // Completely terminated
    };
    
    // Process priority levels
    enum class ProcessPriority : std::int8_t {
        Idle = -20,
        Low = -10,
        Normal = 0,
        High = 10,
        RealTime = 20
    };
    
    // Process scheduling classes
    enum class SchedulingClass : std::uint8_t {
        Normal = 0,      // CFS (Completely Fair Scheduler)
        Batch = 1,       // Batch processing
        Idle = 2,        // Idle tasks
        FIFO = 3,        // Real-time FIFO
        RoundRobin = 4,  // Real-time round-robin
        Deadline = 5     // Deadline scheduling
    };
    
    // Process capabilities (Linux-style)
    enum class Capability : std::uint64_t {
        None = 0,
        ChownFiles = 1ULL << 0,
        BypassFilePermissions = 1ULL << 1,
        BypassFileOwnership = 1ULL << 2,
        OverrideFileOwnership = 1ULL << 3,
        Kill = 1ULL << 5,
        SetGid = 1ULL << 6,
        SetUid = 1ULL << 7,
        SetPcap = 1ULL << 8,
        LinuxImmutable = 1ULL << 9,
        BindService = 1ULL << 10,
        Broadcast = 1ULL << 11,
        NetAdmin = 1ULL << 12,
        NetRaw = 1ULL << 13,
        IpcLock = 1ULL << 14,
        IpcOwner = 1ULL << 15,
        SysModule = 1ULL << 16,
        SysRawio = 1ULL << 17,
        SysChroot = 1ULL << 18,
        SysPtrace = 1ULL << 19,
        SysPacket = 1ULL << 20,
        SysAdmin = 1ULL << 21,
        SysBoot = 1ULL << 22,
        SysNice = 1ULL << 23,
        SysResource = 1ULL << 24,
        SysTime = 1ULL << 25,
        SysTtyConfig = 1ULL << 26,
        Mknod = 1ULL << 27,
        Lease = 1ULL << 28,
        AuditWrite = 1ULL << 29,
        AuditControl = 1ULL << 30,
        SetFcap = 1ULL << 31,
        MacOverride = 1ULL << 32,
        MacAdmin = 1ULL << 33,
        Syslog = 1ULL << 34,
        WakeAlarm = 1ULL << 35,
        BlockSuspend = 1ULL << 36,
        AuditRead = 1ULL << 37,
        All = 0xFFFFFFFFFFFFFFFFULL
    };
    
    // Enable bitwise operations on Capability
    constexpr Capability operator|(Capability a, Capability b) noexcept {
        return static_cast<Capability>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
    }
    
    constexpr Capability operator&(Capability a, Capability b) noexcept {
        return static_cast<Capability>(static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b));
    }
    
    constexpr Capability operator~(Capability a) noexcept {
        return static_cast<Capability>(~static_cast<std::uint64_t>(a));
    }
    
    // Modern CPU register state for x86_64
    struct CpuRegisters {
        // General purpose registers
        std::uint64_t rax, rbx, rcx, rdx;
        std::uint64_t rsi, rdi, rbp, rsp;
        std::uint64_t r8, r9, r10, r11;
        std::uint64_t r12, r13, r14, r15;
        
        // Instruction and flags
        std::uint64_t rip;
        std::uint64_t rflags;
        
        // Segment registers
        std::uint16_t cs, ds, es, fs, gs, ss;
        
        // Control registers
        std::uint64_t cr0, cr1, cr2, cr3, cr4;
        
        // Debug registers
        std::uint64_t dr0, dr1, dr2, dr3, dr6, dr7;
        
        // Model specific registers
        std::uint64_t msr_kernel_gs_base;
        std::uint64_t msr_user_gs_base;
        std::uint64_t msr_fs_base;
        std::uint64_t msr_gs_base;
        
        // FPU/SSE state (simplified)
        alignas(64) std::array<std::uint8_t, 512> fxsave_area;
        
        // Default initialization
        CpuRegisters() noexcept : rax(0), rbx(0), rcx(0), rdx(0),
                                  rsi(0), rdi(0), rbp(0), rsp(0),
                                  r8(0), r9(0), r10(0), r11(0),
                                  r12(0), r13(0), r14(0), r15(0),
                                  rip(0), rflags(0x202), // IF flag set
                                  cs(0), ds(0), es(0), fs(0), gs(0), ss(0),
                                  cr0(0), cr1(0), cr2(0), cr3(0), cr4(0),
                                  dr0(0), dr1(0), dr2(0), dr3(0), dr6(0), dr7(0),
                                  msr_kernel_gs_base(0), msr_user_gs_base(0),
                                  msr_fs_base(0), msr_gs_base(0),
                                  fxsave_area{} {}
    };
    
    // Process memory layout
    struct MemoryLayout {
        Memory::VirtualAddress code_start;
        Memory::VirtualAddress code_end;
        Memory::VirtualAddress data_start;
        Memory::VirtualAddress data_end;
        Memory::VirtualAddress heap_start;
        Memory::VirtualAddress heap_end;
        Memory::VirtualAddress stack_start;
        Memory::VirtualAddress stack_end;
        Memory::VirtualAddress mmap_start;
        Memory::VirtualAddress mmap_end;
        Memory::VirtualAddress kernel_start;
        Memory::VirtualAddress kernel_end;
        
        [[nodiscard]] bool is_valid_address(Memory::VirtualAddress addr) const noexcept {
            return (addr >= code_start && addr < code_end) ||
                   (addr >= data_start && addr < data_end) ||
                   (addr >= heap_start && addr < heap_end) ||
                   (addr >= stack_start && addr < stack_end) ||
                   (addr >= mmap_start && addr < mmap_end);
        }
        
        [[nodiscard]] std::size_t total_size() const noexcept {
            return (code_end - code_start) +
                   (data_end - data_start) +
                   (heap_end - heap_start) +
                   (stack_end - stack_start) +
                   (mmap_end - mmap_start);
        }
    };
    
    // Process resource limits
    struct ResourceLimits {
        std::size_t max_memory;
        std::size_t max_open_files;
        std::size_t max_threads;
        std::size_t max_cpu_time;
        std::size_t max_stack_size;
        std::size_t max_core_file_size;
        std::size_t max_resident_set_size;
        std::size_t max_processes;
        std::size_t max_locked_memory;
        std::size_t max_address_space;
        std::size_t max_file_size;
        std::size_t max_pending_signals;
        std::size_t max_message_queue_size;
        std::size_t max_nice_priority;
        std::size_t max_realtime_priority;
        std::size_t max_realtime_timeout;
        
        // Default limits
        static constexpr ResourceLimits default_limits() noexcept {
            return ResourceLimits{
                .max_memory = 1024 * 1024 * 1024, // 1GB
                .max_open_files = 1024,
                .max_threads = 1024,
                .max_cpu_time = std::numeric_limits<std::size_t>::max(),
                .max_stack_size = 8 * 1024 * 1024, // 8MB
                .max_core_file_size = 0,
                .max_resident_set_size = 1024 * 1024 * 1024, // 1GB
                .max_processes = 1024,
                .max_locked_memory = 64 * 1024, // 64KB
                .max_address_space = std::numeric_limits<std::size_t>::max(),
                .max_file_size = std::numeric_limits<std::size_t>::max(),
                .max_pending_signals = 1024,
                .max_message_queue_size = 1024 * 1024, // 1MB
                .max_nice_priority = 20,
                .max_realtime_priority = 99,
                .max_realtime_timeout = 1000000 // 1 second in microseconds
            };
        }
    };
    
    // Process timing information
    struct ProcessTiming {
        std::chrono::steady_clock::time_point creation_time;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::nanoseconds user_time{0};
        std::chrono::nanoseconds system_time{0};
        std::chrono::nanoseconds total_time{0};
        std::chrono::nanoseconds sleep_time{0};
        std::uint64_t context_switches{0};
        std::uint64_t voluntary_context_switches{0};
        std::uint64_t involuntary_context_switches{0};
        std::uint64_t page_faults{0};
        std::uint64_t major_page_faults{0};
        std::uint64_t minor_page_faults{0};
        
        [[nodiscard]] std::chrono::nanoseconds age() const noexcept {
            return std::chrono::steady_clock::now() - creation_time;
        }
        
        [[nodiscard]] std::chrono::nanoseconds runtime() const noexcept {
            return user_time + system_time;
        }
        
        [[nodiscard]] double cpu_usage() const noexcept {
            const auto total = age();
            const auto used = runtime();
            return total.count() > 0 ? static_cast<double>(used.count()) / total.count() : 0.0;
        }
    };
    
    // Forward declarations
    class Process;
    class Thread;
    class ProcessManager;
    class Scheduler;
    class ProcessGroup;
    class Session;
    
    // Process entry point concept
    template<typename T>
    concept ProcessEntryPoint = std::invocable<T> || std::invocable<T, int, char**>;
    
    // Process result type
    template<typename T>
    using ProcessResult = compat::expected<T, ProcessError>;
    
    // Process handle with RAII
    class ProcessHandle {
    private:
        ProcessId pid_;
        bool owns_process_;
        
    public:
        explicit ProcessHandle(ProcessId pid, bool owns = true) noexcept
            : pid_(pid), owns_process_(owns) {}
        
        ~ProcessHandle() noexcept {
            if (owns_process_ && pid_ != INVALID_PID) {
                // Cleanup will be handled by ProcessManager
            }
        }
        
        // Move-only semantics
        ProcessHandle(const ProcessHandle&) = delete;
        ProcessHandle& operator=(const ProcessHandle&) = delete;
        
        ProcessHandle(ProcessHandle&& other) noexcept
            : pid_(other.pid_), owns_process_(other.owns_process_) {
            other.pid_ = INVALID_PID;
            other.owns_process_ = false;
        }
        
        ProcessHandle& operator=(ProcessHandle&& other) noexcept {
            if (this != &other) {
                pid_ = other.pid_;
                owns_process_ = other.owns_process_;
                other.pid_ = INVALID_PID;
                other.owns_process_ = false;
            }
            return *this;
        }
        
        [[nodiscard]] ProcessId get() const noexcept { return pid_; }
        [[nodiscard]] bool valid() const noexcept { return pid_ != INVALID_PID; }
        
        ProcessId release() noexcept {
            owns_process_ = false;
            return pid_;
        }
        
        void reset(ProcessId new_pid = INVALID_PID) noexcept {
            pid_ = new_pid;
            owns_process_ = true;
        }
    };
    
    // Modern Process class with C++20 features
    class Process {
    private:
        ProcessId pid_;
        ProcessId parent_pid_;
        std::atomic<ProcessState> state_{ProcessState::Created};
        std::string name_;
        std::vector<std::string> command_line_;
        std::unordered_map<std::string, std::string> environment_;
        
        // CPU state
        CpuRegisters registers_;
        std::atomic<ProcessPriority> priority_{ProcessPriority::Normal};
        SchedulingClass scheduling_class_{SchedulingClass::Normal};
        std::atomic<std::int32_t> nice_value_{0};
        
        // Memory management
        MemoryLayout memory_layout_;
        Memory::VirtualAddress page_directory_;
        ResourceLimits limits_;
        
        // Security
        UserId user_id_;
        GroupId group_id_;
        std::vector<GroupId> supplementary_groups_;
        std::bitset<64> capabilities_;
        
        // Timing and statistics
        ProcessTiming timing_;
        
        // Threads
        std::vector<std::unique_ptr<Thread>> threads_;
        ThreadId main_thread_id_;
        
        // File descriptors
        std::vector<std::optional<int>> file_descriptors_;
        
        // Signal handling
        std::bitset<64> pending_signals_;
        std::bitset<64> blocked_signals_;
        std::array<std::function<void(int)>, 64> signal_handlers_;
        
        // Inter-process communication
        std::vector<ProcessId> children_;
        std::optional<int> exit_code_;
        
        // Synchronization
        mutable std::mutex mutex_;
        std::condition_variable state_changed_;
        
    public:
        // Constructors
        explicit Process(ProcessId pid, std::string_view name = "");
        Process(ProcessId pid, ProcessId parent_pid, std::string_view name = "");
        
        // Destructor
        ~Process() noexcept = default;
        
        // Move-only semantics
        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;
        Process(Process&&) = default;
        Process& operator=(Process&&) = default;
        
        // Basic properties
        [[nodiscard]] ProcessId id() const noexcept { return pid_; }
        [[nodiscard]] ProcessId parent_id() const noexcept { return parent_pid_; }
        [[nodiscard]] ProcessState state() const noexcept { return state_.load(); }
        [[nodiscard]] const std::string& name() const noexcept { return name_; }
        [[nodiscard]] const std::vector<std::string>& command_line() const noexcept { return command_line_; }
        
        // State management
        void set_state(ProcessState new_state) noexcept {
            std::lock_guard lock(mutex_);
            state_.store(new_state);
            state_changed_.notify_all();
        }
        
        [[nodiscard]] bool is_running() const noexcept {
            return state_.load() == ProcessState::Running;
        }
        
        [[nodiscard]] bool is_terminated() const noexcept {
            const auto s = state_.load();
            return s == ProcessState::Zombie || s == ProcessState::Terminated;
        }
        
        // Priority and scheduling
        [[nodiscard]] ProcessPriority priority() const noexcept { return priority_.load(); }
        void set_priority(ProcessPriority new_priority) noexcept { priority_.store(new_priority); }
        
        [[nodiscard]] SchedulingClass scheduling_class() const noexcept { return scheduling_class_; }
        void set_scheduling_class(SchedulingClass new_class) noexcept { scheduling_class_ = new_class; }
        
        [[nodiscard]] std::int32_t nice_value() const noexcept { return nice_value_.load(); }
        void set_nice_value(std::int32_t value) noexcept { nice_value_.store(value); }
        
        // Memory management
        [[nodiscard]] const MemoryLayout& memory_layout() const noexcept { return memory_layout_; }
        [[nodiscard]] Memory::VirtualAddress page_directory() const noexcept { return page_directory_; }
        [[nodiscard]] const ResourceLimits& limits() const noexcept { return limits_; }
        
        // CPU state
        [[nodiscard]] const CpuRegisters& registers() const noexcept { return registers_; }
        [[nodiscard]] CpuRegisters& registers() noexcept { return registers_; }
        
        // Security
        [[nodiscard]] UserId user_id() const noexcept { return user_id_; }
        [[nodiscard]] GroupId group_id() const noexcept { return group_id_; }
        [[nodiscard]] bool has_capability(Capability cap) const noexcept {
            return capabilities_.test(static_cast<std::size_t>(cap));
        }
        
        // Timing and statistics
        [[nodiscard]] const ProcessTiming& timing() const noexcept { return timing_; }
        [[nodiscard]] ProcessTiming& timing() noexcept { return timing_; }
        
        // Thread management
        [[nodiscard]] std::size_t thread_count() const noexcept { return threads_.size(); }
        [[nodiscard]] ThreadId main_thread_id() const noexcept { return main_thread_id_; }
        
        // Children management
        [[nodiscard]] const std::vector<ProcessId>& children() const noexcept { return children_; }
        void add_child(ProcessId child_pid) {
            std::lock_guard lock(mutex_);
            children_.push_back(child_pid);
        }
        
        // Exit code
        [[nodiscard]] std::optional<int> exit_code() const noexcept { return exit_code_; }
        void set_exit_code(int code) noexcept { exit_code_ = code; }
        
        // Wait for state change
        template<typename Predicate>
        void wait_for_state_change(Predicate pred) const {
            std::unique_lock lock(mutex_);
            state_changed_.wait(lock, pred);
        }
        
        // Comparison operators
        auto operator<=>(const Process& other) const noexcept {
            return pid_ <=> other.pid_;
        }
        
        bool operator==(const Process& other) const noexcept {
            return pid_ == other.pid_;
        }
    };
    
    // Process creation parameters
    struct ProcessCreateInfo {
        std::string name;
        std::vector<std::string> command_line;
        std::unordered_map<std::string, std::string> environment;
        std::string working_directory;
        UserId user_id = 0;
        GroupId group_id = 0;
        std::vector<GroupId> supplementary_groups;
        Capability capabilities = Capability::None;
        ProcessPriority priority = ProcessPriority::Normal;
        SchedulingClass scheduling_class = SchedulingClass::Normal;
        ResourceLimits limits = ResourceLimits::default_limits();
        std::size_t stack_size = DEFAULT_STACK_SIZE;
        std::size_t heap_size = DEFAULT_HEAP_SIZE;
        bool inherit_handles = false;
        bool create_new_session = false;
        std::optional<ProcessId> parent_pid;
    };
    
    // Modern process management functions with C++20 features
    
    /// Initialize the process management subsystem
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> init() noexcept;
    
    /// Create a new process
    /// @param entry_point Process entry point function
    /// @param create_info Process creation parameters
    /// @return Process handle or error
    template<ProcessEntryPoint EntryPoint>
    [[nodiscard]] ProcessResult<ProcessHandle> create_process(
        EntryPoint&& entry_point,
        const ProcessCreateInfo& create_info = {}
    ) noexcept;
    
    /// Create a new process from executable file
    /// @param executable_path Path to executable file
    /// @param create_info Process creation parameters
    /// @return Process handle or error
    [[nodiscard]] ProcessResult<ProcessHandle> create_process_from_file(
        std::string_view executable_path,
        const ProcessCreateInfo& create_info = {}
    ) noexcept;
    
    /// Terminate a process
    /// @param pid Process ID to terminate
    /// @param exit_code Exit code for the process
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> terminate_process(ProcessId pid, int exit_code = 0) noexcept;
    
    /// Kill a process immediately
    /// @param pid Process ID to kill
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> kill_process(ProcessId pid) noexcept;
    
    /// Get process information
    /// @param pid Process ID
    /// @return Process reference or error
    [[nodiscard]] ProcessResult<std::reference_wrapper<Process>> get_process(ProcessId pid) noexcept;
    
    /// Get process state
    /// @param pid Process ID
    /// @return Process state or error
    [[nodiscard]] ProcessResult<ProcessState> get_process_state(ProcessId pid) noexcept;
    
    /// Get current process ID
    /// @return Current process ID
    [[nodiscard]] ProcessId get_current_process_id() noexcept;
    
    /// Get current process
    /// @return Current process reference
    [[nodiscard]] Process& get_current_process() noexcept;
    
    /// Get total number of processes
    /// @return Process count
    [[nodiscard]] std::size_t get_process_count() noexcept;
    
    /// Get list of all process IDs
    /// @return Vector of process IDs
    [[nodiscard]] std::vector<ProcessId> get_all_process_ids() noexcept;
    
    /// Wait for process to terminate
    /// @param pid Process ID to wait for
    /// @param timeout Maximum time to wait
    /// @return Exit code or error
    [[nodiscard]] ProcessResult<int> wait_for_process(
        ProcessId pid,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max()
    ) noexcept;
    
    /// Wait for any child process to terminate
    /// @param timeout Maximum time to wait
    /// @return Pair of process ID and exit code, or error
    [[nodiscard]] ProcessResult<std::pair<ProcessId, int>> wait_for_any_child(
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max()
    ) noexcept;
    
    /// Schedule next process to run
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> schedule() noexcept;
    
    /// Yield current process
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> yield() noexcept;
    
    /// Sleep for specified duration
    /// @param duration Time to sleep
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> sleep(std::chrono::nanoseconds duration) noexcept;
    
    /// Set process priority
    /// @param pid Process ID
    /// @param priority New priority
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> set_process_priority(ProcessId pid, ProcessPriority priority) noexcept;
    
    /// Set process scheduling class
    /// @param pid Process ID
    /// @param sched_class New scheduling class
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> set_process_scheduling_class(ProcessId pid, SchedulingClass sched_class) noexcept;
    
    /// Clone current process (fork)
    /// @return Child process ID in parent, 0 in child, or error
    [[nodiscard]] ProcessResult<ProcessId> fork() noexcept;
    
    /// Replace current process with new program (exec)
    /// @param executable_path Path to executable
    /// @param args Command line arguments
    /// @param env Environment variables
    /// @return This function does not return on success, error on failure
    [[nodiscard]] ProcessResult<void> exec(
        std::string_view executable_path,
        std::span<const std::string_view> args = {},
        std::span<const std::string_view> env = {}
    ) noexcept;
    
    /// Exit current process
    /// @param exit_code Exit code
    /// @return This function does not return
    [[noreturn]] void exit(int exit_code = 0) noexcept;
    
    /// Send signal to process
    /// @param pid Process ID
    /// @param signal Signal number
    /// @return Success or error code
    [[nodiscard]] ProcessResult<void> send_signal(ProcessId pid, int signal) noexcept;
    
    /// Get process memory usage
    /// @param pid Process ID
    /// @return Memory usage in bytes or error
    [[nodiscard]] ProcessResult<std::size_t> get_memory_usage(ProcessId pid) noexcept;
    
    /// Get process CPU usage
    /// @param pid Process ID
    /// @return CPU usage percentage or error
    [[nodiscard]] ProcessResult<double> get_cpu_usage(ProcessId pid) noexcept;
    
    /// Context switching functions
    namespace Context {
        /// Switch to another process
        /// @param from Current process
        /// @param to Target process
        /// @return Success or error code
        [[nodiscard]] ProcessResult<void> switch_to(Process& from, Process& to) noexcept;
        
        /// Save current CPU state
        /// @param process Process to save state to
        /// @return Success or error code
        [[nodiscard]] ProcessResult<void> save_state(Process& process) noexcept;
        
        /// Restore CPU state
        /// @param process Process to restore state from
        /// @return Success or error code
        [[nodiscard]] ProcessResult<void> restore_state(const Process& process) noexcept;
    }
    
    /// System call implementations
    namespace SystemCalls {
        /// System call: exit
        /// @param exit_code Exit code
        /// @return This function does not return
        [[noreturn]] void sys_exit(int exit_code) noexcept;
        
        /// System call: fork
        /// @return Child PID in parent, 0 in child, or error
        [[nodiscard]] ProcessResult<ProcessId> sys_fork() noexcept;
        
        /// System call: exec
        /// @param pathname Path to executable
        /// @param argv Command line arguments
        /// @param envp Environment variables
        /// @return Success or error code
        [[nodiscard]] ProcessResult<void> sys_exec(
            const char* pathname,
            char* const argv[],
            char* const envp[]
        ) noexcept;
        
        /// System call: wait
        /// @param pid Process ID to wait for (-1 for any child)
        /// @param status Pointer to store exit status
        /// @param options Wait options
        /// @return Process ID that terminated or error
        [[nodiscard]] ProcessResult<ProcessId> sys_wait(
            ProcessId pid,
            int* status,
            int options
        ) noexcept;
        
        /// System call: getpid
        /// @return Current process ID
        [[nodiscard]] ProcessId sys_getpid() noexcept;
        
        /// System call: getppid
        /// @return Parent process ID
        [[nodiscard]] ProcessId sys_getppid() noexcept;
        
        /// System call: kill
        /// @param pid Process ID
        /// @param sig Signal number
        /// @return Success or error code
        [[nodiscard]] ProcessResult<void> sys_kill(ProcessId pid, int sig) noexcept;
        
        /// System call: signal
        /// @param sig Signal number
        /// @param handler Signal handler function
        /// @return Previous handler or error
        [[nodiscard]] ProcessResult<void(*)(int)> sys_signal(int sig, void(*handler)(int)) noexcept;
    }
    
    /// Debug and introspection functions
    #ifdef DEBUG
    void dump_process_table() noexcept;
    void dump_process_info(ProcessId pid) noexcept;
    void dump_scheduler_state() noexcept;
    bool validate_process_state() noexcept;
    void print_process_tree() noexcept;
    #endif
    
    /// Process coroutine support
    namespace Coroutines {
        /// Coroutine task type for processes
        template<typename T = void>
        struct ProcessTask {
            struct promise_type {
                ProcessTask get_return_object() noexcept {
                    return ProcessTask{std::coroutine_handle<promise_type>::from_promise(*this)};
                }
                std::suspend_never initial_suspend() noexcept { return {}; }
                std::suspend_never final_suspend() noexcept { return {}; }
                void unhandled_exception() noexcept { std::terminate(); }
                
                template<typename U>
                void return_value(U&& value) noexcept {
                    result = std::forward<U>(value);
                }
                
                void return_void() noexcept {}
                
                std::optional<T> result;
            };
            
            std::coroutine_handle<promise_type> handle;
            
            explicit ProcessTask(std::coroutine_handle<promise_type> h) noexcept : handle(h) {}
            
            ~ProcessTask() noexcept {
                if (handle) {
                    handle.destroy();
                }
            }
            
            // Move-only
            ProcessTask(const ProcessTask&) = delete;
            ProcessTask& operator=(const ProcessTask&) = delete;
            
            ProcessTask(ProcessTask&& other) noexcept : handle(other.handle) {
                other.handle = nullptr;
            }
            
            ProcessTask& operator=(ProcessTask&& other) noexcept {
                if (this != &other) {
                    if (handle) {
                        handle.destroy();
                    }
                    handle = other.handle;
                    other.handle = nullptr;
                }
                return *this;
            }
            
            [[nodiscard]] bool done() const noexcept {
                return handle && handle.done();
            }
            
            void resume() const noexcept {
                if (handle && !handle.done()) {
                    handle.resume();
                }
            }
            
            [[nodiscard]] std::optional<T> get_result() const noexcept {
                if (handle && handle.done()) {
                    return handle.promise().result;
                }
                return std::nullopt;
            }
        };
        
        /// Yield control to scheduler
        struct YieldAwaitable {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<>) noexcept {
                yield();
            }
            void await_resume() noexcept {}
        };
        
        /// Sleep for specified duration
        struct SleepAwaitable {
            std::chrono::nanoseconds duration;
            
            explicit SleepAwaitable(std::chrono::nanoseconds d) noexcept : duration(d) {}
            
            bool await_ready() noexcept { return duration == std::chrono::nanoseconds::zero(); }
            void await_suspend(std::coroutine_handle<>) noexcept {
                sleep(duration);
            }
            void await_resume() noexcept {}
        };
        
        /// Create yield awaitable
        [[nodiscard]] YieldAwaitable co_yield() noexcept {
            return YieldAwaitable{};
        }
        
        /// Create sleep awaitable
        [[nodiscard]] SleepAwaitable co_sleep(std::chrono::nanoseconds duration) noexcept {
            return SleepAwaitable{duration};
        }
    }
    
} // namespace Process 