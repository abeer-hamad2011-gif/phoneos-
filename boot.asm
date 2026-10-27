MAGIC    equ 0x1BADB002
FLAGS    equ 0
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom: resb 16384
stack_top:

section .data
align 8
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

section .text
global _start
extern kernel_main
_start:
    cli
    mov esp, stack_top
    lgdt [gdt_descriptor]
    jmp 0x08:reload_cs
reload_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    call kernel_main
    cli
hang:
    hlt
    jmp hang
