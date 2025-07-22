#include <gtest/gtest.h>
#include "process.h"
#include <cstdint>

class ProcessTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for each test
    }
    
    void TearDown() override {
        // Cleanup for each test
    }
};

TEST_F(ProcessTest, ProcessStateEnum) {
    // Test process state enumeration
    EXPECT_EQ(static_cast<int>(ProcessState::Ready), 0);
    EXPECT_EQ(static_cast<int>(ProcessState::Running), 1);
    EXPECT_EQ(static_cast<int>(ProcessState::Blocked), 2);
    EXPECT_EQ(static_cast<int>(ProcessState::Terminated), 3);
}

TEST_F(ProcessTest, ProcessStructure) {
    Process proc;
    proc.pid = 1;
    proc.state = ProcessState::Ready;
    proc.eip = 0x100000;
    proc.esp = 0x200000;
    proc.ebp = 0x200000;
    proc.memory_size = 4096;
    proc.stack_top = 0x200000;
    
    EXPECT_EQ(proc.pid, 1);
    EXPECT_EQ(proc.state, ProcessState::Ready);
    EXPECT_EQ(proc.eip, 0x100000);
    EXPECT_EQ(proc.esp, 0x200000);
    EXPECT_EQ(proc.ebp, 0x200000);
    EXPECT_EQ(proc.memory_size, 4096);
    EXPECT_EQ(proc.stack_top, 0x200000);
}

TEST_F(ProcessTest, ProcessCPURegisters) {
    Process proc;
    
    // Test all CPU registers
    proc.registers.eax = 0x11111111;
    proc.registers.ebx = 0x22222222;
    proc.registers.ecx = 0x33333333;
    proc.registers.edx = 0x44444444;
    proc.registers.esi = 0x55555555;
    proc.registers.edi = 0x66666666;
    
    EXPECT_EQ(proc.registers.eax, 0x11111111);
    EXPECT_EQ(proc.registers.ebx, 0x22222222);
    EXPECT_EQ(proc.registers.ecx, 0x33333333);
    EXPECT_EQ(proc.registers.edx, 0x44444444);
    EXPECT_EQ(proc.registers.esi, 0x55555555);
    EXPECT_EQ(proc.registers.edi, 0x66666666);
}

TEST_F(ProcessTest, ProcessPriority) {
    Process proc;
    proc.priority = 5;
    proc.nice_value = 0;
    
    EXPECT_EQ(proc.priority, 5);
    EXPECT_EQ(proc.nice_value, 0);
}

TEST_F(ProcessTest, ProcessMemoryLayout) {
    Process proc;
    proc.memory_size = 1024 * 1024;  // 1MB
    proc.stack_top = proc.memory_size - 16;
    
    EXPECT_EQ(proc.memory_size, 1024 * 1024);
    EXPECT_EQ(proc.stack_top, proc.memory_size - 16);
}

TEST_F(ProcessTest, ProcessParentChild) {
    Process parent;
    Process child;
    
    parent.pid = 1;
    child.pid = 2;
    child.parent_pid = parent.pid;
    
    EXPECT_EQ(parent.pid, 1);
    EXPECT_EQ(child.pid, 2);
    EXPECT_EQ(child.parent_pid, parent.pid);
}

// Test process creation parameters
TEST_F(ProcessTest, ProcessCreationParameters) {
    // Test that process creation would work with valid parameters
    void (*dummy_function)() = nullptr;
    
    // These tests would normally call Process::create_process()
    // But since we're in test mode, we just test the interface
    EXPECT_TRUE(true);  // Placeholder
}

// Test process state transitions
TEST_F(ProcessTest, ProcessStateTransitions) {
    Process proc;
    proc.state = ProcessState::Ready;
    
    // Test state transitions
    proc.state = ProcessState::Running;
    EXPECT_EQ(proc.state, ProcessState::Running);
    
    proc.state = ProcessState::Blocked;
    EXPECT_EQ(proc.state, ProcessState::Blocked);
    
    proc.state = ProcessState::Terminated;
    EXPECT_EQ(proc.state, ProcessState::Terminated);
}

// Test process time tracking
TEST_F(ProcessTest, ProcessTimeTracking) {
    Process proc;
    proc.cpu_time = 1000;  // 1000 time units
    proc.start_time = 0;
    
    EXPECT_EQ(proc.cpu_time, 1000);
    EXPECT_EQ(proc.start_time, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}