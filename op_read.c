/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <string.h>
#include <sys/types.h>

#include "common.h"
#include "disk.h"
#include "e4flib.h"
#include "inode.h"
#include "logging.h"
#include "ops.h"


/* We truncate the read size if it exceeds the limits of the file. */
static size_t truncate_size(struct ext4_inode *inode, size_t size, off_t offset)
{
    DEBUG("Truncate? %zd/%d", offset + size, inode->i_size_lo);

    if (offset >= inode->i_size_lo) {
        return 0;
    }

    if ((offset + size) >= inode->i_size_lo) {
        DEBUG("Truncating read(2) to %d bytes", inode->i_size_lo);
        return inode->i_size_lo - offset;
    }

    return size;
}

/* This function reads all necessary data until the offset is aligned */
static size_t first_read(struct ext4_inode *inode, char *buf, size_t size, off_t offset)
{
    /* Reason for the -1 is that offset = 0 and size = BLOCK_SIZE is all on the
     * same block.  Meaning that byte at offset + size is not actually read. */
    uint32_t end_lblock = (offset + (size - 1)) / BLOCK_SIZE;
    uint32_t start_lblock = offset / BLOCK_SIZE;
    uint32_t start_block_off = offset % BLOCK_SIZE;

    /* If the size is zero, or we are already aligned, skip over this */
    if (size == 0) return 0;
    if (start_block_off == 0) return 0;

    uint64_t start_pblock = inode_get_data_pblock(inode, start_lblock, NULL);

    /* Check if all the read request lays on the same block */
    if (start_lblock == end_lblock) {
        disk_read(BLOCKS2BYTES(start_pblock) + start_block_off, size, buf);
        return size;
    } else {
        size_t size_to_block_end = ALIGN_TO_BLOCKSIZE(offset) - offset;
        ASSERT((offset + size_to_block_end) % BLOCK_SIZE == 0);

        disk_read(BLOCKS2BYTES(start_pblock) + start_block_off, size_to_block_end, buf);
        return size_to_block_end;
    }
}

int e4f_read(const char *path, char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi)
{
    UNUSED(fi);
    struct ext4_inode *inode;
    size_t ret = 0;
    uint32_t extent_len;

    DEBUG("read(%s, buf, %jd, %zd, fi)", path, size, offset);

    if (e4flib_lookup_path(path, &inode) != 0) {
        return -1;
    }

    size = truncate_size(inode, size, offset);
    ret = first_read(inode, buf, size, offset);

    buf += ret;
    offset += ret;

    for (int lblock = offset / BLOCK_SIZE; size > ret; lblock += extent_len) {
        uint64_t pblock = inode_get_data_pblock(inode, lblock, &extent_len);
        struct disk_ctx read_ctx;

        ASSERT(pblock);

        disk_ctx_create(&read_ctx, BLOCKS2BYTES(pblock), BLOCK_SIZE, extent_len);
        ret += disk_ctx_read(&read_ctx, size - ret, buf);
        DEBUG("Read %zd/%zd bytes from %d consecutive blocks", ret, size, extent_len);

        buf += ret;
    }

    /* We always read as many bytes as requested (after initial truncation) */
    ASSERT(size == ret);
    return ret;
}
