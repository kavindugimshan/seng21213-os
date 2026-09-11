/* =============================================================================
 * SENG21213-OS :: RAM Disk
 * File   : kernel/ramdisk.h
 * L12 - A 1 MB byte array in BSS standing in for a real block device.
 * fs.c never touches the array directly - it only reads/writes whole
 * blocks through this API, the same way a real file system talks to a
 * disk driver rather than poking sectors itself.
 * ============================================================================*/
#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

#define RAMDISK_SIZE  (1024u * 1024u)   /* 1 MB, per the assignment spec */
#define BLOCK_SIZE    4096u
#define TOTAL_BLOCKS  (RAMDISK_SIZE / BLOCK_SIZE)   /* 256 blocks */

/* A 1 MB static array does not fit as a normal .bss variable: the kernel is
 * loaded at 0x10000 and boot.asm sets the stack up at only 0x90000 (512 KB
 * of headroom) - a 1 MB array there would grow straight past the stack and
 * corrupt it. Instead the RAM disk lives at this fixed physical address,
 * well above the kernel and its stack, and kernel_main() tells the PMM
 * (pmm_reserve_range()) never to hand these frames out to anything else. */
#define RAMDISK_PHYS_ADDR 0x200000u   /* 2 MB */

void ramdisk_init(void);
void ramdisk_read_block(uint32_t block_num, void *buf);
void ramdisk_write_block(uint32_t block_num, const void *buf);

#endif /* RAMDISK_H */
