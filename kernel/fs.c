/* =============================================================================
 * SENG21213-OS :: RAM Disk File System
 * File   : kernel/fs.c
 *
 * The superblock, directory, both bitmaps and the inode table are kept as
 * plain static structs for convenience, and mirrored out to their assigned
 * ramdisk blocks (fs_sync_metadata()) every time they change - so the
 * layout on the "disk" always matches what the superblock describes, the
 * same way a real file system keeps its on-disk structures consistent.
 * Only file DATA is read/written block-by-block on demand, since that is
 * the part that is actually too big to just keep resident.
 *
 * Simplification: fs_read()/fs_write() have no persistent seek position -
 * every read starts at byte 0, and every write replaces the whole file.
 * That is all the 'cat'/'write' shell commands ever need, and it avoids an
 * open-file-table with per-descriptor offsets that nothing here would use.
 * ============================================================================*/
#include "fs.h"

#define SB_MAGIC 0x53454E47u   /* 'SENG', so a corrupted superblock is obvious */

#define SB_BLOCK           0u
#define DIR_BLOCK          1u
#define BLOCK_BITMAP_BLOCK 2u
#define INODE_BITMAP_BLOCK 3u
#define INODE_TABLE_BLOCK  4u
#define DATA_START_BLOCK   5u

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t block_bitmap_block;
    uint32_t inode_bitmap_block;
    uint32_t inode_table_block;
    uint32_t data_start_block;
} superblock_t;

typedef struct {
    char     name[FS_MAX_NAME];
    uint32_t inode;
    uint8_t  used;
} dirent_t;

typedef struct {
    uint32_t size;
    uint32_t direct[FS_DIRECT_PTRS];
    uint8_t  used;
} inode_t;

static superblock_t sb;
static dirent_t     directory[FS_MAX_DIRENTS];
static inode_t      inodes[FS_MAX_INODES];
static uint8_t       block_bitmap[TOTAL_BLOCKS / 8];
static uint8_t       inode_bitmap[FS_MAX_INODES / 8];

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

/* ---------------------------------------------------------------------------
 * Mirror the in-memory metadata onto the ramdisk, one block each - keeps
 * the "disk" honest about the layout the superblock advertises.
 * --------------------------------------------------------------------------*/
static void fs_sync_metadata(void) {
    uint8_t buf[BLOCK_SIZE];

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) buf[i] = 0;
    *(superblock_t *)buf = sb;
    ramdisk_write_block(SB_BLOCK, buf);

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) buf[i] = 0;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) ((dirent_t *)buf)[i] = directory[i];
    ramdisk_write_block(DIR_BLOCK, buf);

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) buf[i] = 0;
    for (uint32_t i = 0; i < TOTAL_BLOCKS / 8; i++) buf[i] = block_bitmap[i];
    ramdisk_write_block(BLOCK_BITMAP_BLOCK, buf);

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) buf[i] = 0;
    for (uint32_t i = 0; i < FS_MAX_INODES / 8; i++) buf[i] = inode_bitmap[i];
    ramdisk_write_block(INODE_BITMAP_BLOCK, buf);

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) buf[i] = 0;
    for (uint32_t i = 0; i < FS_MAX_INODES; i++) ((inode_t *)buf)[i] = inodes[i];
    ramdisk_write_block(INODE_TABLE_BLOCK, buf);
}

void fs_init(void) {
    ramdisk_init();

    sb.magic               = SB_MAGIC;
    sb.total_blocks        = TOTAL_BLOCKS;
    sb.total_inodes        = FS_MAX_INODES;
    sb.block_bitmap_block  = BLOCK_BITMAP_BLOCK;
    sb.inode_bitmap_block  = INODE_BITMAP_BLOCK;
    sb.inode_table_block   = INODE_TABLE_BLOCK;
    sb.data_start_block    = DATA_START_BLOCK;

    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        directory[i].used = 0;
        directory[i].name[0] = 0;
        directory[i].inode = 0;
    }
    for (uint32_t i = 0; i < FS_MAX_INODES; i++) {
        inodes[i].used = 0;
        inodes[i].size = 0;
        for (uint32_t j = 0; j < FS_DIRECT_PTRS; j++) inodes[i].direct[j] = 0;
    }
    for (uint32_t i = 0; i < TOTAL_BLOCKS / 8; i++) block_bitmap[i] = 0;
    for (uint32_t i = 0; i < FS_MAX_INODES / 8; i++) inode_bitmap[i] = 0;

    /* Blocks 0..DATA_START_BLOCK-1 hold metadata, not file data - reserve them. */
    for (uint32_t b = 0; b < DATA_START_BLOCK; b++) {
        block_bitmap[b / 8] |= (uint8_t)(1u << (b % 8));
    }

    fs_sync_metadata();
}

static int find_dirent(const char *name) {
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        if (directory[i].used && str_eq(directory[i].name, name)) return (int)i;
    }
    return -1;
}

static int find_free_dirent(void) {
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        if (!directory[i].used) return (int)i;
    }
    return -1;
}

static int alloc_inode(void) {
    for (uint32_t i = 0; i < FS_MAX_INODES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].size = 0;
            for (uint32_t j = 0; j < FS_DIRECT_PTRS; j++) inodes[i].direct[j] = 0;
            inode_bitmap[i / 8] |= (uint8_t)(1u << (i % 8));
            return (int)i;
        }
    }
    return -1;
}

static void free_inode(int idx) {
    if (idx < 0 || (uint32_t)idx >= FS_MAX_INODES) return;
    inodes[idx].used = 0;
    inodes[idx].size = 0;
    inode_bitmap[idx / 8] &= (uint8_t)~(1u << (idx % 8));
}

static int alloc_block(void) {
    for (uint32_t b = DATA_START_BLOCK; b < TOTAL_BLOCKS; b++) {
        if (!(block_bitmap[b / 8] & (1u << (b % 8)))) {
            block_bitmap[b / 8] |= (uint8_t)(1u << (b % 8));
            return (int)b;
        }
    }
    return -1;   /* ramdisk full */
}

static void free_block(uint32_t b) {
    if (b < DATA_START_BLOCK || b >= TOTAL_BLOCKS) return;
    block_bitmap[b / 8] &= (uint8_t)~(1u << (b % 8));
}

static void free_all_blocks_of(int ino) {
    for (uint32_t j = 0; j < FS_DIRECT_PTRS; j++) {
        if (inodes[ino].direct[j]) {
            free_block(inodes[ino].direct[j]);
            inodes[ino].direct[j] = 0;
        }
    }
}

int fs_open(const char *name, int create) {
    int d = find_dirent(name);
    if (d >= 0) return (int)directory[d].inode;
    if (!create) return -1;

    int ino = alloc_inode();
    if (ino < 0) return -1;

    int slot = find_free_dirent();
    if (slot < 0) { free_inode(ino); return -1; }

    uint32_t i = 0;
    for (; i < FS_MAX_NAME - 1 && name[i]; i++) directory[slot].name[i] = name[i];
    directory[slot].name[i] = '\0';
    directory[slot].inode = (uint32_t)ino;
    directory[slot].used  = 1;

    fs_sync_metadata();
    return ino;
}

int fs_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || (uint32_t)fd >= FS_MAX_INODES || !inodes[fd].used) return -1;

    uint32_t to_read = inodes[fd].size;
    if (to_read > count) to_read = count;

    uint8_t block[BLOCK_SIZE];
    uint32_t done = 0;
    for (uint32_t i = 0; i < FS_DIRECT_PTRS && done < to_read; i++) {
        if (!inodes[fd].direct[i]) break;
        ramdisk_read_block(inodes[fd].direct[i], block);

        uint32_t chunk = to_read - done;
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        for (uint32_t k = 0; k < chunk; k++) ((uint8_t *)buf)[done + k] = block[k];
        done += chunk;
    }
    return (int)done;
}

int fs_write(int fd, const void *buf, uint32_t count) {
    if (fd < 0 || (uint32_t)fd >= FS_MAX_INODES || !inodes[fd].used) return -1;
    if (count > FS_MAX_FILE_SIZE) count = FS_MAX_FILE_SIZE;

    free_all_blocks_of(fd);   /* whole-file overwrite - simplest correct semantics */

    uint32_t needed = (count + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t written = 0;
    for (uint32_t i = 0; i < needed; i++) {
        int b = alloc_block();
        if (b < 0) break;   /* ramdisk full - keep whatever fit so far */
        inodes[fd].direct[i] = (uint32_t)b;

        uint8_t block[BLOCK_SIZE];
        for (uint32_t k = 0; k < BLOCK_SIZE; k++) block[k] = 0;
        uint32_t chunk = count - written;
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        for (uint32_t k = 0; k < chunk; k++) block[k] = ((const uint8_t *)buf)[written + k];
        ramdisk_write_block((uint32_t)b, block);
        written += chunk;
    }
    inodes[fd].size = written;

    fs_sync_metadata();
    return (int)written;
}

int fs_close(int fd) {
    if (fd < 0 || (uint32_t)fd >= FS_MAX_INODES) return -1;
    return 0;   /* nothing to flush - every write already syncs metadata */
}

int fs_unlink(const char *name) {
    int d = find_dirent(name);
    if (d < 0) return -1;

    int ino = (int)directory[d].inode;
    free_all_blocks_of(ino);
    free_inode(ino);

    directory[d].used = 0;
    directory[d].name[0] = '\0';

    fs_sync_metadata();
    return 0;
}

uint32_t fs_list(char names[][FS_MAX_NAME], uint32_t *sizes, uint32_t max_entries) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS && n < max_entries; i++) {
        if (!directory[i].used) continue;

        uint32_t k = 0;
        for (; k < FS_MAX_NAME - 1 && directory[i].name[k]; k++) names[n][k] = directory[i].name[k];
        names[n][k] = '\0';
        sizes[n] = inodes[directory[i].inode].size;
        n++;
    }
    return n;
}
