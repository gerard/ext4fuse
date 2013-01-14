/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include "disk.h"
#include "extents.h"
#include "logging.h"
#include "super.h"

/* Calculates the physical block from a given logical block and extent */
static uint64_t extent_get_block_from_ees(struct ext4_extent *ee, uint32_t n_ee, uint32_t lblock, uint32_t *len)
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
            if (len) *len = ee[i].ee_block + ee[i].ee_len - lblock;
            break;
        }
    }

    if (n_ee == i) {
        DEBUG("Extent [%d] doesn't contain block", block_ext_index);
        return 0;
    } else {
        DEBUG("Block located [%d:%d]", block_ext_index, block_ext_offset);
        return ee[block_ext_index].ee_start_lo + block_ext_offset;
    }
}

/* Fetches a block that stores extent info and returns an array of extents
 * _with_ its header. */
static void *extent_get_extents_in_block(uint32_t block)
{
    struct ext4_extent_header eh;
    void *exts;

    disk_read(BLOCKS2BYTES(block), sizeof(struct ext4_extent_header), &eh);

    uint32_t extents_len = eh.eh_entries * sizeof(struct ext4_extent)
                                         + sizeof(struct ext4_extent_header);

    exts = malloc(extents_len);

    disk_read(BLOCKS2BYTES(block), extents_len, exts);

    return exts;
}

/* Returns the physical block number */
uint64_t extent_get_pblock(void *extents, uint32_t lblock, uint32_t *len)
{
    struct ext4_extent_header *eh = extents;
    struct ext4_extent *ee_array;
    uint64_t ret;

    ASSERT(eh->eh_magic == EXT4_EXT_MAGIC);

    if (eh->eh_depth == 0) {
        ee_array = extents + sizeof(struct ext4_extent_header);
        ret = extent_get_block_from_ees(ee_array, eh->eh_entries, lblock, len);
    } else {
        struct ext4_extent_idx *ei_array = extents + sizeof(struct ext4_extent_header);
        struct ext4_extent_idx *recurse_ei = NULL;

        for (int i = 0; i < eh->eh_entries; i++) {
            ei_array = extents + sizeof(struct ext4_extent_header);
            ASSERT(ei_array[i].ei_leaf_hi == 0);

            if (ei_array[i].ei_block > lblock) {
                break;
            }

            recurse_ei = &ei_array[i];
        }

        ASSERT(recurse_ei);

        void *leaf_extents = extent_get_extents_in_block(recurse_ei->ei_leaf_lo);
        ret = extent_get_pblock(leaf_extents, lblock, len);
        free(leaf_extents);
    }

    return ret;
}
