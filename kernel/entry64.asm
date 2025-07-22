; ==============================================================================
; entry64.asm - 64-bit kernel entry point
; ==============================================================================
; This file contains the 64-bit assembly entry point for the kernel.
; It's called by the UEFI bootloader after boot services have been exited.

BITS 64

; External symbols
extern kernel_main64           ; C++ kernel main function
extern __bss_start            ; Start of BSS section
extern __bss_end              ; End of BSS section
extern __stack_top            ; Top of kernel stack

; Kernel entry point called by UEFI bootloader
section .text
global _start
_start:
    ; At this point we are in 64-bit long mode, called by UEFI bootloader
    ; RDI contains pointer to BOOT_CONFIG structure
    
    ; Disable interrupts during initialization
    cli
    
    ; Save boot configuration pointer
    push rdi
    
    ; Set up new stack
    mov rsp, __stack_top
    mov rbp, rsp
    
    ; Clear direction flag
    cld
    
    ; Clear BSS section
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    shr rcx, 3              ; Convert to qword count
    xor rax, rax
    rep stosq
    
    ; Restore boot configuration pointer
    pop rdi
    
    ; Set up basic CPU features
    call setup_cpu_features
    
    ; Call C++ kernel main with boot config
    call kernel_main64
    
    ; If kernel main returns, halt
    cli
.halt:
    hlt
    jmp .halt

; ==============================================================================
; setup_cpu_features - Initialize basic CPU features
; ==============================================================================
setup_cpu_features:
    push rbp
    mov rbp, rsp
    
    ; Check if SSE is available
    mov eax, 1
    cpuid
    test edx, (1 << 25)     ; Check SSE bit
    jz .no_sse
    
    ; Enable SSE
    mov rax, cr0
    and rax, ~(1 << 2)      ; Clear CR0.EM
    or rax, (1 << 1)        ; Set CR0.MP
    mov cr0, rax
    
    mov rax, cr4
    or rax, (3 << 9)        ; Set CR4.OSFXSR and CR4.OSXMMEXCPT
    mov cr4, rax
    
.no_sse:
    ; Check if AVX is available
    mov eax, 1
    cpuid
    test ecx, (1 << 28)     ; Check AVX bit
    jz .no_avx
    
    ; Enable AVX
    mov rax, cr4
    or rax, (1 << 18)       ; Set CR4.OSXSAVE
    mov cr4, rax
    
    ; Enable AVX in XCR0
    xor rcx, rcx
    xgetbv
    or eax, 7               ; Enable SSE, AVX
    xsetbv
    
.no_avx:
    pop rbp
    ret

; ==============================================================================
; GDT and IDT setup for 64-bit mode
; ==============================================================================
section .data

; 64-bit GDT
align 16
gdt64:
    dq 0x0000000000000000   ; Null descriptor
    dq 0x00209A0000000000   ; Kernel code segment (64-bit)
    dq 0x0000920000000000   ; Kernel data segment
    dq 0x0020FA0000000000   ; User code segment (64-bit)
    dq 0x0000F20000000000   ; User data segment
gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64 - 1
    dq gdt64

; ==============================================================================
; Exception handling stubs
; ==============================================================================
section .text

; Macro to create exception handlers
%macro EXCEPTION_HANDLER 1
global exception_handler_%1
exception_handler_%1:
    push qword %1           ; Push exception number
    jmp common_exception_handler
%endmacro

; Macro to create exception handlers with error code
%macro EXCEPTION_HANDLER_WITH_ERROR 1
global exception_handler_%1
exception_handler_%1:
    push qword %1           ; Push exception number
    jmp common_exception_handler
%endmacro

; Exception handlers
EXCEPTION_HANDLER 0         ; Divide by zero
EXCEPTION_HANDLER 1         ; Debug
EXCEPTION_HANDLER 2         ; NMI
EXCEPTION_HANDLER 3         ; Breakpoint
EXCEPTION_HANDLER 4         ; Overflow
EXCEPTION_HANDLER 5         ; Bound range exceeded
EXCEPTION_HANDLER 6         ; Invalid opcode
EXCEPTION_HANDLER 7         ; Device not available
EXCEPTION_HANDLER_WITH_ERROR 8  ; Double fault
EXCEPTION_HANDLER 9         ; Coprocessor segment overrun
EXCEPTION_HANDLER_WITH_ERROR 10 ; Invalid TSS
EXCEPTION_HANDLER_WITH_ERROR 11 ; Segment not present
EXCEPTION_HANDLER_WITH_ERROR 12 ; Stack fault
EXCEPTION_HANDLER_WITH_ERROR 13 ; General protection fault
EXCEPTION_HANDLER_WITH_ERROR 14 ; Page fault
EXCEPTION_HANDLER 16        ; x87 FPU error
EXCEPTION_HANDLER_WITH_ERROR 17 ; Alignment check
EXCEPTION_HANDLER 18        ; Machine check
EXCEPTION_HANDLER 19        ; SIMD exception

; Common exception handler
common_exception_handler:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Call C++ exception handler
    mov rdi, rsp            ; Pass stack pointer as interrupt frame
    extern handle_exception
    call handle_exception
    
    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Remove exception number from stack
    add rsp, 8
    
    ; Return from interrupt
    iretq

; ==============================================================================
; System call interface
; ==============================================================================
section .text

global syscall_entry
syscall_entry:
    ; Save user registers
    push rcx                ; User RIP
    push r11                ; User RFLAGS
    
    ; Save other registers
    push rax
    push rbx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r12
    push r13
    push r14
    push r15
    
    ; Call C++ system call handler
    ; RAX = system call number
    ; RDI, RSI, RDX, R10, R8, R9 = arguments
    extern handle_syscall
    call handle_syscall
    
    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rbx
    ; RAX contains return value, don't restore
    add rsp, 8              ; Skip saved RAX
    
    ; Restore user context
    pop r11                 ; User RFLAGS
    pop rcx                 ; User RIP
    
    ; Return to user mode
    sysretq

; ==============================================================================
; Utility functions
; ==============================================================================
section .text

; Load GDT
global load_gdt64
load_gdt64:
    lgdt [gdt64_descriptor]
    
    ; Reload segment registers
    mov ax, 0x10            ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far return to reload CS
    push qword 0x08         ; Kernel code segment
    push qword .reload_cs
    retfq
.reload_cs:
    ret

; Load IDT
global load_idt64
load_idt64:
    lidt [rdi]
    ret

; Get current stack pointer
global get_stack_pointer
get_stack_pointer:
    mov rax, rsp
    ret

; Get current instruction pointer
global get_instruction_pointer
get_instruction_pointer:
    mov rax, [rsp]
    ret

; Read control registers
global read_cr0
read_cr0:
    mov rax, cr0
    ret

global read_cr2
read_cr2:
    mov rax, cr2
    ret

global read_cr3
read_cr3:
    mov rax, cr3
    ret

global read_cr4
read_cr4:
    mov rax, cr4
    ret

; Write control registers
global write_cr0
write_cr0:
    mov cr0, rdi
    ret

global write_cr3
write_cr3:
    mov cr3, rdi
    ret

global write_cr4
write_cr4:
    mov cr4, rdi
    ret

; I/O port operations
global inb
inb:
    mov dx, di
    in al, dx
    ret

global outb
outb:
    mov dx, di
    mov ax, si
    out dx, al
    ret

global inw
inw:
    mov dx, di
    in ax, dx
    ret

global outw
outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

global inl
inl:
    mov dx, di
    in eax, dx
    ret

global outl
outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

; Memory barrier
global memory_barrier
memory_barrier:
    mfence
    ret

; CPU identification
global cpuid_supported
cpuid_supported:
    pushfq
    pop rax
    mov rcx, rax
    xor rax, 0x200000       ; Flip ID bit
    push rax
    popfq
    pushfq
    pop rax
    xor rax, rcx
    shr rax, 21
    and rax, 1
    ret

; ==============================================================================
; Data section
; ==============================================================================
section .data

; Initial kernel stack (1MB)
section .bss
align 16
stack_bottom:
    resb 0x100000           ; 1MB stack
__stack_top: