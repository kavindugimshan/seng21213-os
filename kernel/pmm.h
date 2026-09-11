/* =============================================================================
 * SENG21213-OS :: Physical Memory Manager
 * File   : kernel/pmm.h
 * L11 - A bitmap page-frame allocator (1 bit per 4 KB frame), built from
 * the BIOS E820 memory map that boot/boot.asm recorded in real mode.
 * ============================================================================*/
#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PMM_FRAME_SIZE 4096u

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);       /* returns a physical address, or 0 if out of memory */
void     pmm_free_frame(uint32_t paddr);

uint32_t pmm_total_frames(void);
uint32_t pmm_used_frames(void);
uint32_t pmm_free_frames(void);

/* Raw access to the E820 entries boot.asm collected, for the 'mem' command. */
uint32_t pmm_e820_count(void);
void     pmm_e820_get(uint32_t index, uint64_t *base, uint64_t *length, uint32_t *type);

#endif /* PMM_H */
