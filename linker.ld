ENTRY(_start)
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

SECTIONS
{
    /* Higher half kernel mapping - 64-bit virtual address space */
    . = 0xFFFFFFFF80000000;

    /* Keep track of kernel start for memory management */
    __kernel_start = .;

    /* Text section - executable code */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text.boot)  /* Boot-specific code first */
        *(.text)       /* Regular code */
        *(.text.*)     /* Template instantiations */
        . = ALIGN(4K);
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4K);
    }

    /* Constructor and destructor tables for global objects */
    .init_array BLOCK(4K) : ALIGN(4K)
    {
        __init_array_start = .;
        *(.init_array)
        __init_array_end = .;
        . = ALIGN(4K);
    }

    .fini_array BLOCK(4K) : ALIGN(4K)
    {
        __fini_array_start = .;
        *(.fini_array)
        __fini_array_end = .;
        . = ALIGN(4K);
    }

    /* Initialized data */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
        . = ALIGN(4K);
    }

    /* Uninitialized data */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
        . = ALIGN(4K);
    }

    /* Stack space allocation */
    .stack BLOCK(4K) : ALIGN(4K)
    {
        __stack_bottom = .;
        . += 0x100000;  /* 1MB stack space */
        __stack_top = .;
        . = ALIGN(4K);
    }

    /* Heap space allocation */
    .heap BLOCK(4K) : ALIGN(4K)
    {
        __heap_start = .;
        . += 0x1000000;  /* 16MB heap space */
        __heap_end = .;
        . = ALIGN(4K);
    }

    /* Keep track of kernel end for memory management */
    __kernel_end = .;

    /* Debug information (not loaded into memory) */
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line) }
    .debug_str 0 : { *(.debug_str) }
    .debug_ranges 0 : { *(.debug_ranges) }

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note.*)
        *(.comment)
    }
} 