#pragma once

#include "process.h"

namespace Shell {
    /// Start the interactive shell as a process
    /// @return Process handle or error
    [[nodiscard]] Process::ProcessResult<Process::ProcessHandle> start_shell();
    
} // namespace Shell