/* =============================================================================
 * SENG21213-OS :: RAM Disk File System
 * File   : kernel/fs.h
 * L12 - A minimal flat file system, laid out on the ramdisk (kernel/ramdisk.c)
 * exactly as specified:
 *   block 0  - superblock (magic number, block/inode counts, layout)
 *   block 1  - flat directory (array of {name[28], inode})
 *   block 2  - block bitmap
 *   block 3  - inode bitmap
 *   block 4  - inode table
 *   block 5+ - file data
 * Each inode has 8 direct block pointers, so 8 x 4 KB = 32 KB is the
 * largest file this file system can hold.
 * ============================================================================*/
#ifndef FS_H
#define FS_H

#include "../include/types.h"
#include "ramdisk.h"

#define FS_MAX_NAME      28u
#define FS_DIRECT_PTRS   8u
#define FS_MAX_FILE_SIZE (FS_DIRECT_PTRS * BLOCK_SIZE)   /* 32 KB */
#define FS_MAX_INODES    32u
#define FS_MAX_DIRENTS   32u

void fs_init(void);

/* POSIX-style API. A file descriptor here is just its inode index. */
int fs_open(const char *name, int create);            /* -1 if not found and create==0, or table full */
int fs_read(int fd, void *buf, uint32_t count);        /* always reads from the start of the file */
int fs_write(int fd, const void *buf, uint32_t count); /* always OVERWRITES the whole file, clamped to 32 KB */
int fs_close(int fd);
int fs_unlink(const char *name);

/* For the 'ls' command. Returns the number of files found (<= max_entries). */
uint32_t fs_list(char names[][FS_MAX_NAME], uint32_t *sizes, uint32_t max_entries);

#endif /* FS_H */
