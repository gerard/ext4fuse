/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include "ext4_extents.h"

#include "extents.h"
#include "common.h"
#include "disk.h"
#include "logging.h"

/* Calculates the physical block from a given logical block and extent */
static uint64_t extent_get_block_from_ees(struct ext4_extent *ee, int n_ee, uint32_t lblock)
{
    uint32_t block_ext_index = 0;
    uint32_t block_ext_offset = 0;
    uint32_t i;

    DEBUG("Extent contains %d entries", n_ee);
    DEBUG("Looking for LBlock %d", lblock);

    /* Skip to the right extent entry */
    for (i = 0; i < n_ee; i++) {
        ASSERT(ee[i].ee_start_hi == 0);

        if (ee[i].ee_block + ee[i].ee_len > lblock) {
            block_ext_index = i;
            block_ext_offset = lblock - ee[i].ee_block;
            break;
        }
    }
    ASSERT(n_ee != i);

    DEBUG("Block located [%d:%d]", block_ext_index, block_ext_offset);
    return ee[block_ext_index].ee_start_lo + block_ext_offset;
}

/* Fetches a block that stores extent info and returns an array of extents */
static struct ext4_extent *extent_get_eentries_in_block(uint32_t block, int *n_entries)
{
    struct ext4_extent_header ext_h;
    struct ext4_extent *exts;

    disk_read(BLOCKS2BYTES(block), sizeof(struct ext4_extent_header), &ext_h);
    ASSERT(ext_h.eh_depth == 0);

    uint32_t extents_length = ext_h.eh_entries * sizeof(struct ext4_extent);
    exts = malloc(extents_length);

    uint64_t where = BLOCKS2BYTES(block) + sizeof(struct ext4_extent);
    disk_read(where, extents_length, exts);

    if (n_entries) *n_entries = ext_h.eh_entries;
    return exts;
}

/* Returns the physical block number */
uint64_t extent_get_pblock(struct ext4_inode_extent *inode_ext, uint32_t lblock)
{
    ASSERT(inode_ext->eh.eh_magic == EXT4_EXT_MAGIC);
    ASSERT(inode_ext->eh.eh_entries <= 4);

    /* This should be a static assert */
    ASSERT(sizeof(struct ext4_inode_extent) == 60);

    struct ext4_extent *ee_array;
    int n_entries;

    if (inode_ext->eh.eh_depth == 0) {
        ee_array = inode_ext->ee;
        n_entries = inode_ext->eh.eh_entries;
    } else {
        ASSERT(inode_ext->eh.eh_depth == 1);
        ASSERT(inode_ext->eh.eh_entries == 1);
        ASSERT(inode_ext->ei[0].ei_leaf_hi == 0);

        ee_array = extent_get_eentries_in_block(inode_ext->ei[0].ei_leaf_lo, &n_entries);
    }

    return extent_get_block_from_ees(ee_array, n_entries, lblock);
}
