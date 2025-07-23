/**
 * @file shell.cpp
 * @brief Simple kernel shell implementation
 * 
 * This file implements a basic command-line shell that runs as a kernel thread,
 * providing interactive access to the operating system.
 */

#include "../include/process.h"
#include "../include/filesystem.h"
#include "../include/memory.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace Shell {
    // Shell command interface
    struct ShellCommand {
        std::string name;
        std::string description;
        std::function<int(const std::vector<std::string>&)> handler;
    };
    
    // Shell state
    namespace Internal {
        static std::vector<ShellCommand> commands;
        static bool running = false;
        static std::string prompt = "ModernOS> ";
        static std::string input_buffer;
        static constexpr std::size_t MAX_INPUT_LENGTH = 256;
        
        // Simple console input (placeholder - needs keyboard driver)
        std::string read_line() {
            // TODO: Implement proper keyboard input
            // For now, return a dummy command for demonstration
            static int demo_counter = 0;
            demo_counter++;
            
            switch (demo_counter) {
                case 1: return "help";
                case 2: return "ps";
                case 3: return "ls";
                case 4: return "mkdir test_dir";
                case 5: return "cd test_dir";
                case 6: return "pwd";
                case 7: return "cd /";
                case 8: return "ls";
                case 9: return "echo Hello ModernOS!";
                default: return "exit";
            }
        }
        
        // Parse command line into arguments
        std::vector<std::string> parse_command(const std::string& line) {
            std::vector<std::string> args;
            std::istringstream iss(line);
            std::string arg;
            
            while (iss >> arg) {
                args.push_back(arg);
            }
            
            return args;
        }
        
        // Find command by name
        const ShellCommand* find_command(const std::string& name) {
            auto it = std::find_if(commands.begin(), commands.end(),
                [&name](const ShellCommand& cmd) { return cmd.name == name; });
            return (it != commands.end()) ? &(*it) : nullptr;
        }
    }
    
    // Built-in shell commands
    namespace Commands {
        int help(const std::vector<std::string>& args) {
            Console::print("Available commands:\n");
            for (const auto& cmd : Internal::commands) {
                Console::print("  ");
                Console::print(cmd.name.c_str());
                Console::print(" - ");
                Console::print(cmd.description.c_str());
                Console::print("\n");
            }
            return 0;
        }
        
        int echo(const std::vector<std::string>& args) {
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (i > 1) Console::print(" ");
                Console::print(args[i].c_str());
            }
            Console::print("\n");
            return 0;
        }
        
        int pwd(const std::vector<std::string>& args) {
            auto cwd = FileSystem::get_current_directory();
            Console::print(cwd.string().c_str());
            Console::print("\n");
            return 0;
        }
        
        int cd(const std::vector<std::string>& args) {
            if (args.size() < 2) {
                auto result = FileSystem::set_current_directory("/");
                if (!result.has_value()) {
                    Console::print("cd: failed to change to /\n");
                    return 1;
                }
            } else {
                auto result = FileSystem::set_current_directory(args[1]);
                if (!result.has_value()) {
                    Console::print("cd: ");
                    Console::print(args[1].c_str());
                    Console::print(": No such directory\n");
                    return 1;
                }
            }
            return 0;
        }
        
        int ls(const std::vector<std::string>& args) {
            std::string path = (args.size() > 1) ? args[1] : ".";
            
            auto result = FileSystem::list_directory(path);
            if (!result.has_value()) {
                Console::print("ls: ");
                Console::print(path.c_str());
                Console::print(": No such directory\n");
                return 1;
            }
            
            const auto& entries = result.value();
            for (const auto& entry : entries) {
                // Skip . and .. for cleaner output unless specifically requested
                if (entry.name == "." || entry.name == "..") {
                    continue;
                }
                
                // Color coding based on type
                if (entry.type == FileSystem::FileType::Directory) {
                    Console::set_color(0x0B); // Light cyan for directories
                } else {
                    Console::set_color(0x0F); // White for files
                }
                
                Console::print(entry.name.c_str());
                
                if (entry.type == FileSystem::FileType::Directory) {
                    Console::print("/");
                }
                
                Console::print("  ");
            }
            Console::set_color(0x0F); // Reset color
            Console::print("\n");
            return 0;
        }
        
        int mkdir(const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Console::print("mkdir: missing directory name\n");
                return 1;
            }
            
            auto result = FileSystem::create_directory(args[1], FileSystem::FilePermissions::Default, false);
            if (!result.has_value()) {
                Console::print("mkdir: ");
                Console::print(args[1].c_str());
                Console::print(": Cannot create directory\n");
                return 1;
            }
            return 0;
        }
        
        int ps(const std::vector<std::string>& args) {
            Console::print("  PID  NAME\n");
            Console::print("-----  --------\n");
            
            auto pids = Process::get_all_process_ids();
            for (auto pid : pids) {
                auto process_result = Process::get_process(pid);
                if (process_result.has_value()) {
                    auto& proc = process_result.value().get();
                    
                    // Print PID
                    if (pid < 10) Console::print("    ");
                    else if (pid < 100) Console::print("   ");
                    else if (pid < 1000) Console::print("  ");
                    else Console::print(" ");
                    Console::print_dec(pid);
                    Console::print("  ");
                    
                    // Print name
                    Console::print(proc.name().c_str());
                    Console::print("\n");
                }
            }
            return 0;
        }
        
        int kill(const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Console::print("kill: missing process ID\n");
                return 1;
            }
            
            try {
                Process::ProcessId pid = std::stoul(args[1]);
                auto result = Process::kill_process(pid);
                if (!result.has_value()) {
                    Console::print("kill: ");
                    Console::print(args[1].c_str());
                    Console::print(": No such process\n");
                    return 1;
                }
            } catch (...) {
                Console::print("kill: ");
                Console::print(args[1].c_str());
                Console::print(": Invalid process ID\n");
                return 1;
            }
            return 0;
        }
        
        int cat(const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Console::print("cat: missing file name\n");
                return 1;
            }
            
            auto result = FileSystem::Convenience::read_text_file(args[1]);
            if (!result.has_value()) {
                Console::print("cat: ");
                Console::print(args[1].c_str());
                Console::print(": No such file\n");
                return 1;
            }
            
            Console::print(result.value().c_str());
            return 0;
        }
        
        int touch(const std::vector<std::string>& args) {
            if (args.size() < 2) {
                Console::print("touch: missing file name\n");
                return 1;
            }
            
            auto result = FileSystem::create_file(args[1], FileSystem::FilePermissions::Default);
            if (!result.has_value()) {
                Console::print("touch: ");
                Console::print(args[1].c_str());
                Console::print(": Cannot create file\n");
                return 1;
            }
            return 0;
        }
        
        int write(const std::vector<std::string>& args) {
            if (args.size() < 3) {
                Console::print("write: usage: write <file> <content>\n");
                return 1;
            }
            
            std::string content;
            for (std::size_t i = 2; i < args.size(); ++i) {
                if (i > 2) content += " ";
                content += args[i];
            }
            
            auto result = FileSystem::Convenience::write_text_file(args[1], content, true);
            if (!result.has_value()) {
                Console::print("write: ");
                Console::print(args[1].c_str());
                Console::print(": Cannot write file\n");
                return 1;
            }
            return 0;
        }
        
        int uname(const std::vector<std::string>& args) {
            Console::print("ModernOS 1.0.0-alpha x86_64\n");
            return 0;
        }
        
        int clear(const std::vector<std::string>& args) {
            Console::clear_screen();
            return 0;
        }
        
        int exit(const std::vector<std::string>& args) {
            Internal::running = false;
            Console::print("Shell exiting...\n");
            return 0;
        }
    }
    
    // Initialize shell commands
    void initialize_commands() {
        Internal::commands = {
            {"help", "Show this help message", Commands::help},
            {"echo", "Echo arguments to output", Commands::echo},
            {"pwd", "Print working directory", Commands::pwd},
            {"cd", "Change directory", Commands::cd},
            {"ls", "List directory contents", Commands::ls},
            {"mkdir", "Create directory", Commands::mkdir},
            {"ps", "Show running processes", Commands::ps},
            {"kill", "Terminate process", Commands::kill},
            {"cat", "Display file contents", Commands::cat},
            {"touch", "Create empty file", Commands::touch},
            {"write", "Write content to file", Commands::write},
            {"uname", "Show system information", Commands::uname},
            {"clear", "Clear screen", Commands::clear},
            {"exit", "Exit shell", Commands::exit}
        };
    }
    
    // Main shell loop
    void shell_main() {
        Console::print("\n");
        Console::set_color(0x0E); // Yellow
        Console::print("========================================\n");
        Console::print("     ModernOS Interactive Shell\n");
        Console::print("========================================\n");
        Console::set_color(0x0F); // White
        Console::print("Type 'help' for available commands.\n\n");
        
        Internal::running = true;
        
        while (Internal::running) {
            // Print prompt
            Console::set_color(0x0A); // Light green
            Console::print(Internal::prompt.c_str());
            Console::set_color(0x0F); // White
            
            // Read command
            std::string line = Internal::read_line();
            Console::print(line.c_str());
            Console::print("\n");
            
            // Parse command
            auto args = Internal::parse_command(line);
            if (args.empty()) {
                continue;
            }
            
            // Find and execute command
            const auto* command = Internal::find_command(args[0]);
            if (command) {
                int result = command->handler(args);
                if (result != 0) {
                    Console::set_color(0x0C); // Light red
                    Console::print("Command failed with exit code ");
                    Console::print_dec(result);
                    Console::print("\n");
                    Console::set_color(0x0F); // White
                }
            } else {
                Console::set_color(0x0C); // Light red
                Console::print(args[0].c_str());
                Console::print(": command not found\n");
                Console::set_color(0x0F); // White
            }
            
            // Yield CPU to other processes
            Process::yield();
        }
        
        Console::print("Shell terminated.\n");
    }
    
    // Start shell as a process
    Process::ProcessResult<Process::ProcessHandle> start_shell() {
        initialize_commands();
        
        Process::ProcessCreateInfo create_info;
        create_info.name = "shell";
        create_info.priority = Process::ProcessPriority::Normal;
        create_info.scheduling_class = Process::SchedulingClass::Normal;
        
        return Process::create_process(shell_main, create_info);
    }
    
} // namespace Shell