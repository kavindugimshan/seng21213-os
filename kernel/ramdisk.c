/* =============================================================================
 * SENG21213-OS :: RAM Disk
 * File   : kernel/ramdisk.c
 * ============================================================================*/
#include "ramdisk.h"

static uint8_t * const disk = (uint8_t *)RAMDISK_PHYS_ADDR;

void ramdisk_init(void) {
    for (uint32_t i = 0; i < RAMDISK_SIZE; i++) disk[i] = 0;
}

void ramdisk_read_block(uint32_t block_num, void *buf) {
    if (block_num >= TOTAL_BLOCKS) return;
    uint8_t *src = &disk[block_num * BLOCK_SIZE];
    uint8_t *dst = (uint8_t *)buf;
    for (uint32_t i = 0; i < BLOCK_SIZE; i++) dst[i] = src[i];
}

void ramdisk_write_block(uint32_t block_num, const void *buf) {
    if (block_num >= TOTAL_BLOCKS) return;
    uint8_t *dst = &disk[block_num * BLOCK_SIZE];
    const uint8_t *src = (const uint8_t *)buf;
    for (uint32_t i = 0; i < BLOCK_SIZE; i++) dst[i] = src[i];
}
