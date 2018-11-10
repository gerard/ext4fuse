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
#include <errno.h>
#include <inttypes.h>

#include "common.h"
#include "disk.h"
#include "inode.h"
#include "logging.h"
#include "ops.h"


/* We truncate the read size if it exceeds the limits of the file. */
static size_t truncate_size(struct ext4_inode *inode, size_t size, size_t offset)
{
    uint64_t inode_size = inode_get_size(inode);

    DEBUG("Truncate? %zd/%d", offset + size, inode_size);

    if (offset >= inode_size) {
        return 0;
    }

    if ((offset + size) >= inode_size) {
        DEBUG("Truncating read(2) to %"PRIu64" bytes", inode_size);
        return inode_size - offset;
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

int op_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    size_t un_offset = (size_t)offset;
    struct ext4_inode inode;
    size_t ret = 0;
    uint32_t extent_len;

    /* Not sure if this is possible at all... */
    ASSERT(offset >= 0);

    DEBUG("read(%s, buf, %zd, %zd, fi->fh=%d)", path, size, un_offset, fi->fh);
    int inode_get_ret = inode_get_by_number(fi->fh, &inode);

    if (inode_get_ret < 0) {
        return inode_get_ret;
    }

    size = truncate_size(&inode, size, un_offset);
    ret = first_read(&inode, buf, size, un_offset);

    buf += ret;
    un_offset += ret;

    for (unsigned int lblock = un_offset / BLOCK_SIZE; size > ret; lblock += extent_len) {
        uint64_t pblock = inode_get_data_pblock(&inode, lblock, &extent_len);
        size_t bytes;

        if (pblock) {
            struct disk_ctx read_ctx;

            disk_ctx_create(&read_ctx, BLOCKS2BYTES(pblock), BLOCK_SIZE, extent_len);
            bytes = disk_ctx_read(&read_ctx, size - ret, buf);
        } else {
            bytes = size - ret;
            if (bytes > BLOCK_SIZE) {
                bytes = BLOCK_SIZE;
            }
            memset(buf,0,bytes);
            DEBUG("sparse file, skipping %d bytes",bytes);
        }
        ret += bytes;
        buf += bytes;
        DEBUG("Read %zd/%zd bytes from %d consecutive blocks", ret, size, extent_len);
    }

    /* We always read as many bytes as requested (after initial truncation) */
    ASSERT(size == ret);
    return ret;
}
