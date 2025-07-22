#include "process.h"
#include "memory.h"
#include "interrupt.h"
#include <string.h>

namespace Process {
    // Maximum number of processes
    constexpr size_t MAX_PROCESSES = 256;

    // Process table
    static Process processes[MAX_PROCESSES];
    static size_t process_count = 0;
    static uint32_t next_pid = 1;
    static Process* current_process = nullptr;

    // Initialize process management
    void init() {
        memset(processes, 0, sizeof(processes));
        process_count = 0;
        next_pid = 1;
        current_process = nullptr;
    }

    // Create a new process
    uint32_t create_process(void (*entry_point)()) {
        if (process_count >= MAX_PROCESSES) {
            return 0;
        }

        // Find free process slot
        Process* proc = nullptr;
        for (size_t i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].state == ProcessState::Terminated) {
                proc = &processes[i];
                break;
            }
        }

        if (!proc) {
            return 0;
        }

        // Initialize process
        proc->pid = next_pid++;
        proc->state = ProcessState::Ready;
        proc->eip = (uint32_t)entry_point;
        
        // Allocate memory for process
        proc->memory_size = 4 * 1024 * 1024; // 4MB initial size
        if (!allocate_process_memory(proc->pid, proc->memory_size)) {
            return 0;
        }

        // Set up initial stack
        proc->stack_top = proc->memory_size - 16;
        proc->esp = proc->stack_top;
        proc->ebp = proc->stack_top;

        process_count++;
        return proc->pid;
    }

    // Terminate a process
    bool terminate_process(uint32_t pid) {
        Process* proc = get_process(pid);
        if (!proc) {
            return false;
        }

        proc->state = ProcessState::Terminated;
        process_count--;

        // Free process memory
        // TODO: Implement memory cleanup

        return true;
    }

    // Schedule next process
    void schedule() {
        if (process_count == 0) {
            return;
        }

        // Simple round-robin scheduling
        Process* next = nullptr;
        for (size_t i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].state == ProcessState::Ready) {
                next = &processes[i];
                break;
            }
        }

        if (next) {
            if (current_process) {
                current_process->state = ProcessState::Ready;
            }
            next->state = ProcessState::Running;
            current_process = next;
            switch_context(next);
        }
    }

    // Get process by PID
    Process* get_process(uint32_t pid) {
        for (size_t i = 0; i < MAX_PROCESSES; i++) {
            if (processes[i].pid == pid) {
                return &processes[i];
            }
        }
        return nullptr;
    }

    // Get process state
    ProcessState get_process_state(uint32_t pid) {
        Process* proc = get_process(pid);
        return proc ? proc->state : ProcessState::Terminated;
    }

    // Get total process count
    size_t get_process_count() {
        return process_count;
    }

    // Allocate memory for a process
    bool allocate_process_memory(uint32_t pid, size_t size) {
        Process* proc = get_process(pid);
        if (!proc) {
            return false;
        }

        // Allocate new memory pages
        void* new_memory = Memory::allocate_page();
        if (!new_memory) {
            return false;
        }

        // Map memory into process address space
        Memory::map_page((uint32_t)new_memory, proc->memory_size);
        proc->memory_size += size;

        return true;
    }

    // Free process memory
    bool free_process_memory(uint32_t pid, void* ptr) {
        Process* proc = get_process(pid);
        if (!proc) {
            return false;
        }

        // TODO: Implement memory freeing
        return true;
    }

    // Switch to another process context
    void switch_context(Process* next) {
        if (!next) {
            return;
        }

        // Save current context
        if (current_process) {
            asm volatile("mov %%esp, %0" : "=m"(current_process->esp));
            asm volatile("mov %%ebp, %0" : "=m"(current_process->ebp));
        }

        // Load next context
        asm volatile("mov %0, %%esp" : : "m"(next->esp));
        asm volatile("mov %0, %%ebp" : : "m"(next->ebp));
        asm volatile("jmp *%0" : : "r"(next->eip));
    }

    // System calls
    void sys_exit(int status) {
        if (current_process) {
            terminate_process(current_process->pid);
        }
    }

    int sys_fork() {
        if (!current_process) {
            return -1;
        }

        // Create new process
        Process* child = get_process(create_process(nullptr));
        if (!child) {
            return -1;
        }

        // Copy parent's memory
        // TODO: Implement memory copying

        return child->pid;
    }

    int sys_exec(const char* path) {
        // TODO: Implement exec
        return -1;
    }

    int sys_wait(int* status) {
        // TODO: Implement wait
        return -1;
    }
}; 