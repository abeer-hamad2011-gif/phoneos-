global idt_load
extern irq_dispatch

; Load the IDT: argument is a pointer to the idt_ptr struct
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

%macro IRQ_STUB 2
global irq%1_stub
irq%1_stub:
    cli
    push dword 0        ; fake error code
    push dword %2       ; interrupt vector number
    jmp irq_common
%endmacro

IRQ_STUB 0, 32          ; timer (IRQ0)
IRQ_STUB 1, 33          ; keyboard (IRQ1)

irq_common:
    pusha               ; save all registers
    mov eax, [esp+32]   ; vector number
    push eax
    call irq_dispatch
    add esp, 4
    popa
    add esp, 8          ; drop error code and vector number
    sti
    iret
