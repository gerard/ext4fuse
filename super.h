#ifndef SUPER_H
#define SUPER_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "common.h"

#define BLOCK_SIZE                              (super_block_size())
#define BLOCKS2BYTES(__blks)                    (((uint64_t)(__blks)) * BLOCK_SIZE)
#define BYTES2BLOCKS(__bytes)                   ((__bytes) / BLOCK_SIZE + ((__bytes) % BLOCK_SIZE ? 1 : 0))

#define MALLOC_BLOCKS(__blks)                   (malloc(BLOCKS2BYTES(__blks)))
#define ALIGN_TO_BLOCKSIZE(__n)                 (ALIGN_TO(__n, BLOCK_SIZE))

#define BOOT_SECTOR_SIZE            0x400


/* struct ext4_super */
uint32_t super_block_size(void);
uint32_t super_inodes_per_group(void);
uint32_t super_inode_size(void);
int super_fill(void);

/* struct ext4_group_desc */
off_t super_group_inode_table_offset(uint32_t inode_num);
int super_group_fill(void);

#endif
