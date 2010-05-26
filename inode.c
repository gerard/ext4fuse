/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <stdlib.h>
#include <string.h>

#include "types/ext4.h"

#include "disk.h"
#include "extents.h"
#include "logging.h"
#include "super.h"


#define MAX_DIRECTED_BLOCK          12
#define INDIRECT_BLOCK_L1           12
#define MAX_INDIRECTED_BLOCK        MAX_DIRECTED_BLOCK + (BLOCK_SIZE / sizeof(uint32_t))

/* Get pblock for a given inode and lblock.  If extent is not NULL, it will
 * store the length of extent, that is, the number of consecutive pblocks
 * that are also consecutive lblocks (not counting the requested one). */
uint64_t inode_get_data_pblock(struct ext4_inode *inode, uint32_t lblock, uint32_t *extent)
{
    if (inode->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_inode_extent *inode_ext = (struct ext4_inode_extent *)&inode->i_block;
        return extent_get_pblock(inode_ext, lblock, extent);
    } else {
        ASSERT(lblock <= BYTES2BLOCKS(inode->i_size_lo));
        if (extent) *extent = 0;

        if (lblock < MAX_DIRECTED_BLOCK) {
            return inode->i_block[lblock];
        }

        if (lblock < MAX_INDIRECTED_BLOCK) {
            uint32_t indirect_block[BLOCK_SIZE];
            uint32_t indirect_index = lblock - MAX_DIRECTED_BLOCK;

            disk_read_block(INDIRECT_BLOCK_L1, indirect_block);
            return indirect_block[indirect_index];
        }

        else {
            /* Handle this later (double-indirected block) */
            ASSERT(0);
        }
    }

    /* Should never reach here */
    ASSERT(0);
    return 0;
}

struct ext4_inode *inode_get(uint32_t inode_num)
{
    if (inode_num == 0) return NULL;
    inode_num--;    /* Inode 0 doesn't exist on disk */

    struct ext4_inode *ret = malloc(super_inode_size());
    memset(ret, 0, super_inode_size());

    off_t inode_off = super_group_inode_table_offset(inode_num);
    inode_off += (inode_num % super_inodes_per_group()) * super_inode_size();

    disk_read(inode_off, super_inode_size(), ret);
    return ret;
}

void inode_put(struct ext4_inode *inode)
{
    free(inode);
}
