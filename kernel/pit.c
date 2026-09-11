/* =============================================================================
 * SENG21213-OS :: i8253 PIT driver
 * File   : kernel/pit.c
 * ============================================================================*/
#include "pit.h"

#define PIT_CH0_PORT    0x40
#define PIT_CMD_PORT    0x43
#define PIT_BASE_FREQ   1193182u   /* the PIT's fixed input clock, in Hz */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

void pit_init(uint32_t freq_hz) {
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQ / freq_hz);

    outb(PIT_CMD_PORT, 0x36); /* channel 0, lobyte/hibyte access, mode 3 (square wave) */
    outb(PIT_CH0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}
