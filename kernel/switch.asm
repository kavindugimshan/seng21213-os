; =============================================================================
; SENG21213-OS :: Context-Switch Stub (IRQ0 handler)
; File   : kernel/switch.asm
; L09 §Context switch - PUSHAD/POPAD save and restore of a process's
; general-purpose registers. This is the ONLY place where a stack switch
; between processes actually happens.
;
; The CPU (via the IDT gate set up in idt.c) already pushed EFLAGS, CS, EIP
; before jumping here, since this is an interrupt gate. We add the rest of
; the register state with PUSHAD, hand the current ESP to the C scheduler,
; and load whatever ESP it gives back - which may belong to a completely
; different process's stack.
; =============================================================================
[BITS 32]
[EXTERN scheduler_tick]
[GLOBAL irq0_stub]

irq0_stub:
    pushad                  ; save EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI of the running process

    push esp                ; arg for scheduler_tick(uint32_t current_esp)
    call scheduler_tick     ; returns (in EAX) the ESP to resume next
    add  esp, 4             ; caller cleans up the argument (cdecl)

    mov  esp, eax           ; <-- the actual context switch: swap stacks

    mov  al, 0x20
    out  0x20, al           ; send End-Of-Interrupt to the master PIC

    popad                   ; restore the (possibly different) process's registers
    iret                    ; pop EIP/CS/EFLAGS - resumes that process
