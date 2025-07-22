#pragma once
#include "compat/cstdint"

namespace Process {
    // Initialize process management
    void init();
    
    // Create a process
    std::uint32_t create_process(void (*entry_point)());
    
    // Schedule processes
    void schedule();
}