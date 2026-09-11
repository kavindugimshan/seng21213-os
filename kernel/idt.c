/* =============================================================================
 * SENG21213-OS :: Interrupt Descriptor Table + 8259 PIC remap
 * File   : kernel/idt.c
 * ============================================================================*/
#include "idt.h"
#include "../include/types.h"

/* I/O ports for the two 8259 Programmable Interrupt Controllers */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* Defined in kernel/switch.asm - the IRQ0 (timer) entry point */
extern void irq0_stub(void);

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].selector    = 0x08;    /* kernel code segment, see boot/boot.asm GDT */
    idt[n].zero        = 0;
    idt[n].type_attr   = 0x8E;    /* present, ring 0, 32-bit interrupt gate */
    idt[n].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
}

/* By default the BIOS maps IRQ0-7 to interrupt vectors 0x08-0x0F, which
 * collide with CPU exceptions (e.g. 0x08 is the Double Fault exception).
 * Remapping moves IRQ0-7 to 0x20-0x27 and IRQ8-15 to 0x28-0x2F so hardware
 * interrupts never collide with CPU exceptions. Standard 8259 init sequence
 * (Stallings Ch.1 / OSDev "8259 PIC"). */
static void pic_remap(void) {
    outb(PIC1_CMD,  0x11); /* ICW1: start init sequence, expect ICW4 */
    outb(PIC2_CMD,  0x11);
    outb(PIC1_DATA, 0x20); /* ICW2: master PIC vector offset -> 0x20 */
    outb(PIC2_DATA, 0x28); /* ICW2: slave PIC vector offset  -> 0x28 */
    outb(PIC1_DATA, 0x04); /* ICW3: tell master a slave sits on IRQ2 */
    outb(PIC2_DATA, 0x02); /* ICW3: tell slave its cascade identity */
    outb(PIC1_DATA, 0x01); /* ICW4: 8086 mode */
    outb(PIC2_DATA, 0x01);

    /* Mask every IRQ except IRQ0 (timer). The keyboard (IRQ1) is still
     * polled, not interrupt-driven, and no other IDT gate has a real
     * handler yet, so anything else must stay masked. */
    outb(PIC1_DATA, 0xFE); /* 1111 1110 - only IRQ0 unmasked */
    outb(PIC2_DATA, 0xFF); /* all slave IRQs masked */
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) idt_set_gate(i, 0);
    idt_set_gate(0x20, (uint32_t)irq0_stub);

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base  = (uint32_t)&idt;
    __asm__ __volatile__("lidt (%0)" : : "r"(&idt_ptr));

    pic_remap();
}
