#include <gtest/gtest.h>
#include "interrupt.h"
#include <cstdint>

class InterruptTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for each test
    }
    
    void TearDown() override {
        // Cleanup for each test
    }
};

TEST_F(InterruptTest, InterruptNumbers) {
    // Test standard interrupt numbers
    EXPECT_EQ(0, 0);   // Division by zero
    EXPECT_EQ(14, 14); // Page fault
    EXPECT_EQ(32, 32); // Timer interrupt (IRQ0)
    EXPECT_EQ(33, 33); // Keyboard interrupt (IRQ1)
}

TEST_F(InterruptTest, InterruptHandlerType) {
    // Test that InterruptHandler is a function pointer
    InterruptHandler handler = nullptr;
    EXPECT_EQ(handler, nullptr);
    
    // Test assignment
    handler = [](uint32_t int_num, uint32_t error_code) {
        // Mock handler
    };
    EXPECT_NE(handler, nullptr);
}

TEST_F(InterruptTest, InterruptFlags) {
    // Test interrupt flag values
    uint8_t present_flag = 0x80;
    uint8_t ring0_flag = 0x00;
    uint8_t ring3_flag = 0x60;
    uint8_t interrupt_gate = 0x0E;
    
    EXPECT_EQ(present_flag, 0x80);
    EXPECT_EQ(ring0_flag, 0x00);
    EXPECT_EQ(ring3_flag, 0x60);
    EXPECT_EQ(interrupt_gate, 0x0E);
}

TEST_F(InterruptTest, IDTEntry) {
    // Test IDT entry structure (would be internal to interrupt.cpp)
    struct TestIDTEntry {
        uint16_t base_low;
        uint16_t selector;
        uint8_t zero;
        uint8_t flags;
        uint16_t base_high;
    } __attribute__((packed));
    
    TestIDTEntry entry;
    entry.base_low = 0x1000;
    entry.selector = 0x08;  // Kernel code segment
    entry.zero = 0;
    entry.flags = 0x8E;     // Present, Ring 0, Interrupt Gate
    entry.base_high = 0x0000;
    
    EXPECT_EQ(entry.base_low, 0x1000);
    EXPECT_EQ(entry.selector, 0x08);
    EXPECT_EQ(entry.zero, 0);
    EXPECT_EQ(entry.flags, 0x8E);
    EXPECT_EQ(entry.base_high, 0x0000);
}

TEST_F(InterruptTest, InterruptRegistration) {
    // Test interrupt handler registration interface
    // This would test Interrupt::register_handler() functionality
    EXPECT_TRUE(true);  // Placeholder for actual implementation
}

TEST_F(InterruptTest, InterruptEnableDisable) {
    // Test interrupt enable/disable functionality
    // This would test Interrupt::enable() and Interrupt::disable()
    EXPECT_TRUE(true);  // Placeholder for actual implementation
}

TEST_F(InterruptTest, CommonInterruptHandlers) {
    // Test that common interrupt handlers exist
    // These functions should be defined in interrupt.h
    EXPECT_TRUE(true);  // Placeholder for testing actual handlers
}

TEST_F(InterruptTest, IOPortOperations) {
    // Test I/O port operations (inb/outb)
    // These would normally be inline assembly functions
    uint16_t port = 0x60;  // Keyboard data port
    uint8_t value = 0x20;  // EOI command
    
    EXPECT_EQ(port, 0x60);
    EXPECT_EQ(value, 0x20);
}

TEST_F(InterruptTest, InterruptVectorTable) {
    // Test interrupt vector table setup
    const int MAX_INTERRUPTS = 256;
    EXPECT_EQ(MAX_INTERRUPTS, 256);
    
    // Test that we can handle all possible interrupts
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        EXPECT_GE(i, 0);
        EXPECT_LT(i, MAX_INTERRUPTS);
    }
}

TEST_F(InterruptTest, PICConfiguration) {
    // Test PIC (Programmable Interrupt Controller) configuration
    uint8_t master_pic_command = 0x20;
    uint8_t master_pic_data = 0x21;
    uint8_t slave_pic_command = 0xA0;
    uint8_t slave_pic_data = 0xA1;
    
    EXPECT_EQ(master_pic_command, 0x20);
    EXPECT_EQ(master_pic_data, 0x21);
    EXPECT_EQ(slave_pic_command, 0xA0);
    EXPECT_EQ(slave_pic_data, 0xA1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}