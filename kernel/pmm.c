/* =============================================================================
 * SENG21213-OS :: Physical Memory Manager
 * File   : kernel/pmm.c
 *
 * boot/boot.asm called BIOS INT 0x15/EAX=0xE820 in real mode (protected
 * mode code cannot call the BIOS) and left the results at two fixed, known
 * addresses:
 *   0x8000  a 16-bit count of entries
 *   0x8004  that many 24-byte E820 entries, back to back
 *
 * Every frame from 1 MB upward that an E820 "usable" (type 1) region
 * covers is tracked by one bit in a bitmap (1 = used/reserved, 0 = free).
 * Memory below 1 MB (BIOS data, the boot sector, this kernel itself, the
 * E820 buffer above) is never touched by the allocator - it is simply
 * never marked free, since the whole kernel is far smaller than 1 MB.
 * ============================================================================*/
#include "pmm.h"

#define PMM_START_ADDR   0x100000u              /* manage RAM from 1 MB up */
#define PMM_START_FRAME  (PMM_START_ADDR / PMM_FRAME_SIZE)
#define MAX_FRAMES       32768u                  /* bitmap covers up to 128 MB */

#define E820_COUNT_ADDR   0x8000u
#define E820_ENTRIES_ADDR 0x8004u
#define E820_TYPE_USABLE  1u

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
} e820_entry_t;

static uint8_t  bitmap[MAX_FRAMES / 8];
static uint32_t frame_limit  = 0;   /* one past the highest usable frame index */
static uint32_t used_frames_count = 0;

static inline void bitmap_set(uint32_t frame)   { bitmap[frame / 8] |=  (uint8_t)(1u << (frame % 8)); }
static inline void bitmap_clear(uint32_t frame) { bitmap[frame / 8] &= (uint8_t)~(1u << (frame % 8)); }
static inline int  bitmap_test(uint32_t frame)  { return (bitmap[frame / 8] >> (frame % 8)) & 1; }

void pmm_init(void) {
    for (uint32_t i = 0; i < MAX_FRAMES / 8; i++) bitmap[i] = 0xFF;  /* all used by default */
    frame_limit = 0;

    uint32_t count = pmm_e820_count();
    for (uint32_t i = 0; i < count; i++) {
        uint64_t base, length; uint32_t type;
        pmm_e820_get(i, &base, &length, &type);
        if (type != E820_TYPE_USABLE) continue;

        uint64_t start = base;
        uint64_t end   = base + length;
        if (start < PMM_START_ADDR) start = PMM_START_ADDR;
        if (end <= start) continue;

        uint32_t first_frame = (uint32_t)(start / PMM_FRAME_SIZE);
        uint32_t last_frame  = (uint32_t)(end / PMM_FRAME_SIZE);   /* exclusive */
        if (last_frame > MAX_FRAMES) last_frame = MAX_FRAMES;

        for (uint32_t f = first_frame; f < last_frame; f++) bitmap_clear(f);
        if (last_frame > frame_limit) frame_limit = last_frame;
    }

    used_frames_count = 0;
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t f = PMM_START_FRAME; f < frame_limit; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames_count++;
            return f * PMM_FRAME_SIZE;
        }
    }
    return 0;   /* out of memory */
}

void pmm_free_frame(uint32_t paddr) {
    uint32_t f = paddr / PMM_FRAME_SIZE;
    if (f < PMM_START_FRAME || f >= frame_limit) return;   /* not ours to free */
    if (bitmap_test(f)) {
        bitmap_clear(f);
        used_frames_count--;
    }
}

void pmm_reserve_range(uint32_t start_addr, uint32_t length) {
    uint32_t first = start_addr / PMM_FRAME_SIZE;
    uint32_t last  = (start_addr + length + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;  /* exclusive, rounded up */
    if (last > MAX_FRAMES) last = MAX_FRAMES;
    for (uint32_t f = first; f < last && f < frame_limit; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames_count++;   /* keep used/free accounting consistent */
        }
    }
}

uint32_t pmm_total_frames(void) {
    return (frame_limit > PMM_START_FRAME) ? (frame_limit - PMM_START_FRAME) : 0;
}

uint32_t pmm_used_frames(void) { return used_frames_count; }
uint32_t pmm_free_frames(void) { return pmm_total_frames() - used_frames_count; }

uint32_t pmm_e820_count(void) {
    return *(volatile uint16_t *)E820_COUNT_ADDR;
}

void pmm_e820_get(uint32_t index, uint64_t *base, uint64_t *length, uint32_t *type) {
    e820_entry_t *entries = (e820_entry_t *)E820_ENTRIES_ADDR;
    *base   = entries[index].base;
    *length = entries[index].length;
    *type   = entries[index].type;
}
