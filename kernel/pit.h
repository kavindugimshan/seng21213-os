/* =============================================================================
 * SENG21213-OS :: i8253 Programmable Interval Timer
 * File   : kernel/pit.h
 * L09 §Timer interrupts - programs PIT channel 0 to fire IRQ0 at a fixed
 * frequency, which drives the round-robin scheduler in scheduler.c.
 * ============================================================================*/
#ifndef PIT_H
#define PIT_H

#include "../include/types.h"

/* Programs channel 0 to generate a square wave at freq_hz (e.g. 100 for a
 * 10 ms tick). Must be called after idt_init() and before sti. */
void pit_init(uint32_t freq_hz);

#endif /* PIT_H */
