BITS 16           ; We start in 16-bit mode
org 0x7C00          ; BIOS loads bootloader at this address

; Initialize segment registers
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7C00      ; Set up stack pointer

; Print welcome message
mov si, welcome_msg
call print_string

; Infinite loop (halt)
jmp $

; Function to print a string
print_string:
    mov ah, 0x0E    ; BIOS teletype function
.loop:
    lodsb           ; Load next character from SI into AL
    test al, al     ; Check if character is 0 (end of string)
    jz .done        ; If zero, we're done
    int 0x10        ; Print character using BIOS interrupt
    jmp .loop       ; Repeat for next character
.done:
    ret

; Data
welcome_msg: db 'Welcome to SimpleOS!', 0

; Boot sector padding
times 510-($-$$) db 0   ; Pad with zeros until 510 bytes
dw 0xAA55              ; Boot signature 