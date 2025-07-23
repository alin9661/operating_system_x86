/**
 * @file process.cpp
 * @brief Modern C++20 process management implementation
 * 
 * This file implements the process management system using C++20 features
 * including concepts, coroutines, and std::expected for error handling.
 */

#include "../include/process.h"
#include "../include/memory.h"
#include "../include/interrupt.h"
#include <algorithm>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace Process {
    // Internal process management state
    namespace Internal {
        // Process table and management
        static std::unordered_map<ProcessId, std::unique_ptr<Process>> process_table;
        static std::atomic<ProcessId> next_pid{INIT_PID + 1};
        static std::atomic<ProcessId> current_pid{KERNEL_PID};
        static std::mutex process_table_mutex;
        
        // Simple round-robin scheduler queue
        static std::vector<ProcessId> ready_queue;
        static std::mutex scheduler_mutex;
        static std::atomic<std::size_t> scheduler_index{0};
        
        // System initialization flag
        static std::atomic<bool> initialized{false};
        
        // Process statistics
        static std::atomic<std::size_t> total_processes{0};
        static std::atomic<std::size_t> running_processes{0};
        
        // Find process by PID (internal, no locking)
        [[nodiscard]] Process* find_process_unlocked(ProcessId pid) noexcept {
            auto it = process_table.find(pid);
            return (it != process_table.end()) ? it->second.get() : nullptr;
        }
        
        // Allocate new PID
        [[nodiscard]] ProcessId allocate_pid() noexcept {
            return next_pid.fetch_add(1);
        }
        
        // Add process to scheduler queue
        void add_to_scheduler_queue(ProcessId pid) noexcept {
            std::lock_guard lock(scheduler_mutex);
            ready_queue.push_back(pid);
        }
        
        // Remove process from scheduler queue
        void remove_from_scheduler_queue(ProcessId pid) noexcept {
            std::lock_guard lock(scheduler_mutex);
            ready_queue.erase(
                std::remove(ready_queue.begin(), ready_queue.end(), pid),
                ready_queue.end()
            );
        }
        
        // Get next process for scheduling
        [[nodiscard]] ProcessId get_next_scheduled_process() noexcept {
            std::lock_guard lock(scheduler_mutex);
            if (ready_queue.empty()) {
                return INVALID_PID;
            }
            
            // Simple round-robin
            auto index = scheduler_index.load() % ready_queue.size();
            scheduler_index.store(index + 1);
            return ready_queue[index];
        }
    }
    
    // Process class implementation
    Process::Process(ProcessId pid, std::string_view name)
        : pid_(pid), parent_pid_(KERNEL_PID), name_(name) {
        timing_.creation_time = std::chrono::steady_clock::now();
        timing_.start_time = timing_.creation_time;
    }
    
    Process::Process(ProcessId pid, ProcessId parent_pid, std::string_view name)
        : pid_(pid), parent_pid_(parent_pid), name_(name) {
        timing_.creation_time = std::chrono::steady_clock::now();
        timing_.start_time = timing_.creation_time;
    }
    
    // Core process management functions
    
    /// Initialize the process management subsystem
    [[nodiscard]] ProcessResult<void> init() noexcept {
        if (Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        try {
            std::lock_guard lock(Internal::process_table_mutex);
            
            // Clear any existing state
            Internal::process_table.clear();
            Internal::ready_queue.clear();
            Internal::next_pid = INIT_PID + 1;
            Internal::current_pid = KERNEL_PID;
            Internal::total_processes = 0;
            Internal::running_processes = 0;
            Internal::scheduler_index = 0;
            
            // Create kernel process
            auto kernel_proc = std::make_unique<Process>(KERNEL_PID, "kernel");
            kernel_proc->set_state(ProcessState::Running);
            kernel_proc->set_priority(ProcessPriority::RealTime);
            kernel_proc->set_scheduling_class(SchedulingClass::FIFO);
            
            Internal::process_table[KERNEL_PID] = std::move(kernel_proc);
            Internal::total_processes = 1;
            Internal::running_processes = 1;
            
            Internal::initialized = true;
            
        } catch (...) {
            return compat::unexpected(ProcessError::InsufficientMemory);
        }
        
        return {};
    }
    
    /// Create a process with a lambda/function entry point
    template<ProcessEntryPoint EntryPoint>
    [[nodiscard]] ProcessResult<ProcessHandle> create_process(
        EntryPoint&& entry_point,
        const ProcessCreateInfo& create_info
    ) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        try {
            ProcessId pid = Internal::allocate_pid();
            auto parent_pid = create_info.parent_pid.value_or(get_current_process_id());
            
            // Create new process
            auto process = std::make_unique<Process>(pid, parent_pid, create_info.name);
            
            // Set up process parameters
            process->set_priority(create_info.priority);
            process->set_scheduling_class(create_info.scheduling_class);
            process->limits_ = create_info.limits;
            
            // Initialize memory layout
            // TODO: Implement proper memory allocation and virtual memory setup
            process->memory_layout_.code_start = 0x400000; // Standard user space start
            process->memory_layout_.code_end = process->memory_layout_.code_start + (1024 * 1024); // 1MB code
            process->memory_layout_.heap_start = process->memory_layout_.code_end;
            process->memory_layout_.heap_end = process->memory_layout_.heap_start + create_info.heap_size;
            process->memory_layout_.stack_end = 0x800000000000ULL; // High virtual address for stack
            process->memory_layout_.stack_start = process->memory_layout_.stack_end - create_info.stack_size;
            
            // Set up CPU registers for entry point
            // For now, just set RIP to the function address
            process->registers_.rip = reinterpret_cast<std::uint64_t>(&entry_point);
            process->registers_.rsp = process->memory_layout_.stack_end - 16; // Leave some space
            process->registers_.rbp = process->registers_.rsp;
            process->registers_.rflags = 0x202; // IF flag set
            
            // Set initial state
            process->set_state(ProcessState::Ready);
            
            // Add to process table
            {
                std::lock_guard lock(Internal::process_table_mutex);
                Internal::process_table[pid] = std::move(process);
                Internal::total_processes.fetch_add(1);
            }
            
            // Add to scheduler queue
            Internal::add_to_scheduler_queue(pid);
            
            // Add as child to parent process
            if (auto parent = Internal::find_process_unlocked(parent_pid)) {
                parent->add_child(pid);
            }
            
            return ProcessHandle(pid, true);
            
        } catch (...) {
            return compat::unexpected(ProcessError::InsufficientMemory);
        }
    }
    
    /// Explicit instantiation for common entry point types
    template ProcessResult<ProcessHandle> create_process(
        std::function<void()>&& entry_point,
        const ProcessCreateInfo& create_info
    ) noexcept;
    
    /// Create a new process from executable file
    [[nodiscard]] ProcessResult<ProcessHandle> create_process_from_file(
        std::string_view executable_path,
        const ProcessCreateInfo& create_info
    ) noexcept {
        // TODO: Implement ELF loading
        return compat::unexpected(ProcessError::UnsupportedOperation);
    }
    
    /// Terminate a process
    [[nodiscard]] ProcessResult<void> terminate_process(ProcessId pid, int exit_code) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        if (pid == KERNEL_PID) {
            return compat::unexpected(ProcessError::PermissionDenied);
        }
        
        std::lock_guard lock(Internal::process_table_mutex);
        auto* process = Internal::find_process_unlocked(pid);
        if (!process) {
            return compat::unexpected(ProcessError::ProcessNotFound);
        }
        
        // Set exit code and state
        process->set_exit_code(exit_code);
        process->set_state(ProcessState::Zombie);
        
        // Remove from scheduler queue
        Internal::remove_from_scheduler_queue(pid);
        
        // Update statistics
        if (process->state() == ProcessState::Running) {
            Internal::running_processes.fetch_sub(1);
        }
        
        return {};
    }
    
    /// Kill a process immediately
    [[nodiscard]] ProcessResult<void> kill_process(ProcessId pid) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        if (pid == KERNEL_PID) {
            return compat::unexpected(ProcessError::PermissionDenied);
        }
        
        std::lock_guard lock(Internal::process_table_mutex);
        auto* process = Internal::find_process_unlocked(pid);
        if (!process) {
            return compat::unexpected(ProcessError::ProcessNotFound);
        }
        
        // Immediately terminate
        process->set_state(ProcessState::Terminated);
        
        // Remove from scheduler queue
        Internal::remove_from_scheduler_queue(pid);
        
        // Clean up from process table
        Internal::process_table.erase(pid);
        Internal::total_processes.fetch_sub(1);
        
        if (process->state() == ProcessState::Running) {
            Internal::running_processes.fetch_sub(1);
        }
        
        return {};
    }
    
    /// Get process information
    [[nodiscard]] ProcessResult<std::reference_wrapper<Process>> get_process(ProcessId pid) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        std::lock_guard lock(Internal::process_table_mutex);
        auto* process = Internal::find_process_unlocked(pid);
        if (!process) {
            return compat::unexpected(ProcessError::ProcessNotFound);
        }
        
        return std::reference_wrapper<Process>(*process);
    }
    
    /// Get process state
    [[nodiscard]] ProcessResult<ProcessState> get_process_state(ProcessId pid) noexcept {
        auto result = get_process(pid);
        if (!result.has_value()) {
            return compat::unexpected(result.error());
        }
        
        return result.value().get().state();
    }
    
    /// Get current process ID
    [[nodiscard]] ProcessId get_current_process_id() noexcept {
        return Internal::current_pid.load();
    }
    
    /// Get current process
    [[nodiscard]] Process& get_current_process() noexcept {
        ProcessId current_pid = get_current_process_id();
        
        std::lock_guard lock(Internal::process_table_mutex);
        auto* process = Internal::find_process_unlocked(current_pid);
        
        // This should never happen in a well-formed system
        if (!process) {
            // Fallback to kernel process
            process = Internal::find_process_unlocked(KERNEL_PID);
        }
        
        return *process;
    }
    
    /// Get total number of processes
    [[nodiscard]] std::size_t get_process_count() noexcept {
        return Internal::total_processes.load();
    }
    
    /// Get list of all process IDs
    [[nodiscard]] std::vector<ProcessId> get_all_process_ids() noexcept {
        std::vector<ProcessId> pids;
        
        std::lock_guard lock(Internal::process_table_mutex);
        pids.reserve(Internal::process_table.size());
        
        for (const auto& [pid, process] : Internal::process_table) {
            pids.push_back(pid);
        }
        
        return pids;
    }
    
    /// Wait for process to terminate
    [[nodiscard]] ProcessResult<int> wait_for_process(
        ProcessId pid,
        std::chrono::milliseconds timeout
    ) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            {
                std::lock_guard lock(Internal::process_table_mutex);
                auto* process = Internal::find_process_unlocked(pid);
                if (!process) {
                    return compat::unexpected(ProcessError::ProcessNotFound);
                }
                
                if (process->is_terminated()) {
                    auto exit_code = process->exit_code();
                    return exit_code.value_or(-1);
                }
            }
            
            // Check timeout
            if (timeout != std::chrono::milliseconds::max()) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (elapsed >= timeout) {
                    return compat::unexpected(ProcessError::TimeoutExpired);
                }
            }
            
            // Yield CPU
            yield();
        }
    }
    
    /// Schedule next process to run
    [[nodiscard]] ProcessResult<void> schedule() noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(ProcessError::InvalidState);
        }
        
        ProcessId next_pid = Internal::get_next_scheduled_process();
        if (next_pid == INVALID_PID) {
            // No processes to schedule, keep current
            return {};
        }
        
        ProcessId current_pid = Internal::current_pid.load();
        
        // Don't switch if we're already running the target process
        if (next_pid == current_pid) {
            return {};
        }
        
        std::lock_guard lock(Internal::process_table_mutex);
        
        auto* current_process = Internal::find_process_unlocked(current_pid);
        auto* next_process = Internal::find_process_unlocked(next_pid);
        
        if (!next_process) {
            return compat::unexpected(ProcessError::ProcessNotFound);
        }
        
        // Update process states
        if (current_process && current_process->state() == ProcessState::Running) {
            current_process->set_state(ProcessState::Ready);
        }
        
        next_process->set_state(ProcessState::Running);
        Internal::current_pid = next_pid;
        
        // Perform context switch
        // TODO: Implement actual CPU context switching
        return Context::switch_to(*current_process, *next_process);
    }
    
    /// Yield current process
    [[nodiscard]] ProcessResult<void> yield() noexcept {
        // Simple yield: just call scheduler
        return schedule();
    }
    
    /// Sleep for specified duration
    [[nodiscard]] ProcessResult<void> sleep(std::chrono::nanoseconds duration) noexcept {
        // For now, simple busy wait
        // TODO: Implement proper timer-based sleep
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < duration) {
            yield();
        }
        return {};
    }
    
    /// Fork implementation (simplified)
    [[nodiscard]] ProcessResult<ProcessId> fork() noexcept {
        // TODO: Implement proper process memory copying
        return compat::unexpected(ProcessError::UnsupportedOperation);
    }
    
    /// Context switching functions
    namespace Context {
        /// Switch to another process
        [[nodiscard]] ProcessResult<void> switch_to(Process& from, Process& to) noexcept {
            // TODO: Implement actual x86_64 context switching
            // For now, just save basic state
            save_state(from);
            restore_state(to);
            return {};
        }
        
        /// Save current CPU state
        [[nodiscard]] ProcessResult<void> save_state(Process& process) noexcept {
            // TODO: Implement proper register saving
            // This would involve assembly code to save all CPU registers
            return {};
        }
        
        /// Restore CPU state
        [[nodiscard]] ProcessResult<void> restore_state(const Process& process) noexcept {
            // TODO: Implement proper register restoration
            // This would involve assembly code to restore all CPU registers
            return {};
        }
    }
    
    /// System call implementations
    namespace SystemCalls {
        [[noreturn]] void sys_exit(int exit_code) noexcept {
            ProcessId current_pid = get_current_process_id();
            terminate_process(current_pid, exit_code);
            
            // Schedule next process
            schedule();
            
            // Should not reach here, but if we do, halt
            while (true) {
                asm volatile("hlt");
            }
        }
        
        [[nodiscard]] ProcessResult<ProcessId> sys_fork() noexcept {
            return fork();
        }
        
        [[nodiscard]] ProcessId sys_getpid() noexcept {
            return get_current_process_id();
        }
        
        [[nodiscard]] ProcessId sys_getppid() noexcept {
            ProcessId current_pid = get_current_process_id();
            auto process_result = get_process(current_pid);
            if (!process_result.has_value()) {
                return INVALID_PID;
            }
            
            return process_result.value().get().parent_id();
        }
    }
    
} // namespace Process