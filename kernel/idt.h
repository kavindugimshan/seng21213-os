/* =============================================================================
 * SENG21213-OS :: Interrupt Descriptor Table
 * File   : kernel/idt.h
 * L09 §Interrupts - minimal IDT so the CPU can dispatch IRQ0 (the PIT timer)
 * to our context-switch stub. Stage 1 only needs vector 0x20 (IRQ0); other
 * vectors are left as empty gates, since no other interrupt source is
 * enabled yet (the PIC is configured to mask everything except IRQ0).
 * ============================================================================*/
#ifndef IDT_H
#define IDT_H

void idt_init(void);

#endif /* IDT_H */
