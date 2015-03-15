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
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>

#include "dcache.h"
#include "disk.h"
#include "extents.h"
#include "inode.h"
#include "logging.h"
#include "super.h"


/* These #defines are only relevant for ext2/3 style block indexing */
#define ADDRESSES_IN_IND_BLOCK      (BLOCK_SIZE / sizeof(uint32_t))
#define ADDRESSES_IN_DIND_BLOCK     (ADDRESSES_IN_IND_BLOCK * ADDRESSES_IN_IND_BLOCK)
#define ADDRESSES_IN_TIND_BLOCK     (ADDRESSES_IN_DIND_BLOCK * ADDRESSES_IN_IND_BLOCK)
#define MAX_IND_BLOCK               (EXT4_NDIR_BLOCKS + ADDRESSES_IN_IND_BLOCK)
#define MAX_DIND_BLOCK              (MAX_IND_BLOCK + ADDRESSES_IN_DIND_BLOCK)
#define MAX_TIND_BLOCK              (MAX_DIND_BLOCK + ADDRESSES_IN_TIND_BLOCK)

#define ROOT_INODE_N                2
#define IS_PATH_SEPARATOR(__c)      ((__c) == '/')


static uint32_t __inode_get_data_pblock_ind(uint32_t lblock, uint32_t index_block)
{
    ASSERT(lblock < ADDRESSES_IN_IND_BLOCK);
    return disk_read_u32(BLOCKS2BYTES(index_block) + lblock * sizeof(uint32_t));
}

static uint32_t __inode_get_data_pblock_dind(uint32_t lblock, uint32_t dindex_block)
{
    ASSERT(lblock < ADDRESSES_IN_DIND_BLOCK);

    uint32_t index_block_offset_in_dindex = (lblock / ADDRESSES_IN_IND_BLOCK) * sizeof(uint32_t);
    lblock %= ADDRESSES_IN_IND_BLOCK;

    uint32_t index_block = disk_read_u32(BLOCKS2BYTES(dindex_block) + index_block_offset_in_dindex);

    return __inode_get_data_pblock_ind(lblock, index_block);
}

static uint32_t __inode_get_data_pblock_tind(uint32_t lblock, uint32_t tindex_block)
{
    ASSERT(lblock < ADDRESSES_IN_TIND_BLOCK);

    uint32_t dindex_block_offset_in_tindex = (lblock / ADDRESSES_IN_DIND_BLOCK) * sizeof(uint32_t);
    lblock %= ADDRESSES_IN_DIND_BLOCK;

    uint32_t dindex_block = disk_read_u32(BLOCKS2BYTES(tindex_block) + dindex_block_offset_in_tindex);

    return __inode_get_data_pblock_dind(lblock, dindex_block);
}

/* Get pblock for a given inode and lblock.  If extent is not NULL, it will
 * store the length of extent, that is, the number of consecutive pblocks
 * that are also consecutive lblocks (not counting the requested one). */
uint64_t inode_get_data_pblock(struct ext4_inode *inode, uint32_t lblock, uint32_t *extent_len)
{
    if (extent_len) *extent_len = 1;

    if (inode->i_flags & EXT4_EXTENTS_FL) {
        return extent_get_pblock(&inode->i_block, lblock, extent_len);
    } else {
        ASSERT(lblock <= BYTES2BLOCKS(inode_get_size(inode)));

        if (lblock < EXT4_NDIR_BLOCKS) {
            return inode->i_block[lblock];
        } else if (lblock < MAX_IND_BLOCK) {
            uint32_t index_block = inode->i_block[EXT4_IND_BLOCK];
            return __inode_get_data_pblock_ind(lblock - EXT4_NDIR_BLOCKS, index_block);
        } else if (lblock < MAX_DIND_BLOCK) {
            uint32_t dindex_block = inode->i_block[EXT4_DIND_BLOCK];
            return __inode_get_data_pblock_dind(lblock - MAX_IND_BLOCK, dindex_block);
        } else if (lblock < MAX_TIND_BLOCK) {
            uint32_t tindex_block = inode->i_block[EXT4_TIND_BLOCK];
            return __inode_get_data_pblock_tind(lblock - MAX_DIND_BLOCK, tindex_block);
        } else {
            /* File-system corruption? */
            ASSERT(0);
        }
    }

    /* Should never reach here */
    ASSERT(0);
    return 0;
}

static void dir_ctx_update(struct ext4_inode *inode, uint32_t lblock, struct inode_dir_ctx *ctx)
{
    uint64_t dir_pblock = inode_get_data_pblock(inode, lblock, NULL);
    disk_read_block(dir_pblock, ctx->buf);
    ctx->lblock = lblock;
}

struct inode_dir_ctx *inode_dir_ctx_get(void)
{
    return malloc(sizeof(struct inode_dir_ctx) + BLOCK_SIZE);
}

void inode_dir_ctx_reset(struct inode_dir_ctx *ctx, struct ext4_inode *inode)
{
    dir_ctx_update(inode, 0, ctx);
}

void inode_dir_ctx_put(struct inode_dir_ctx *ctx)
{
    free(ctx);
}

struct ext4_dir_entry_2 *inode_dentry_get(struct ext4_inode *inode, off_t offset, struct inode_dir_ctx *ctx)
{
    uint32_t lblock = offset / BLOCK_SIZE;
    uint32_t blk_offset = offset % BLOCK_SIZE;
    uint64_t inode_size = inode_get_size(inode);
    size_t un_offset = (size_t)offset;

    DEBUG("%zd/%"PRIu64"", un_offset, inode_size);
    ASSERT(inode_size >= un_offset);
    if (inode_size == un_offset) {
        return NULL;
    }

    if (lblock == ctx->lblock) {
        return (struct ext4_dir_entry_2 *)&ctx->buf[blk_offset];
    } else {
        dir_ctx_update(inode, lblock, ctx);
        return inode_dentry_get(inode, un_offset, ctx);
    }
}

int inode_get_by_number(uint32_t n, struct ext4_inode *inode)
{
    if (n == 0) return -ENOENT;
    n--;    /* Inode 0 doesn't exist on disk */

    off_t off = super_group_inode_table_offset(n);
    off += (n % super_inodes_per_group()) * super_inode_size();

    /* If on-disk inode is ext3 type, it will be smaller than the struct.  EXT4
     * inodes, on the other hand, are double size, but the struct still doesn't
     * have fields for all of them. */
    disk_read(off, MIN(super_inode_size(), sizeof(struct ext4_inode)), inode);
    return 0;
}

static uint8_t get_path_token_len(const char *path)
{
    uint8_t len = 0;
    while (path[len] != '/' && path[len]) len++;
    return len;
}

static struct dcache_entry *get_cached_inode_num(const char **path)
{
    struct dcache_entry *next = NULL;
    struct dcache_entry *ret;

    do {
        if (**path == '/') *path = *path + 1; /* Skip over the slash */
        uint8_t path_len = get_path_token_len(*path);
        ret = next;

        if (path_len == 0) {
            return ret;
        }

        next = dcache_lookup(ret, *path, path_len);
        if (next) *path += path_len;
    } while(next != NULL);

    return ret;
}

static const char *skip_trailing_backslash(const char *path)
{
    while (IS_PATH_SEPARATOR(*path)) path++;
    return path;
}

uint32_t inode_get_idx_by_path(const char *path)
{
    struct inode_dir_ctx *dctx = inode_dir_ctx_get();
    uint32_t inode_idx = 0;
    struct ext4_inode inode;

    /* Paths from fuse are always absolute */
    assert(IS_PATH_SEPARATOR(path[0]));

    DEBUG("Looking up: %s", path);

    struct dcache_entry *dc_entry = get_cached_inode_num(&path);
    inode_idx = dcache_get_inode(dc_entry);

    DEBUG("Looking up after dcache: %s", path);

    do {
        uint32_t offset = 0;
        struct ext4_dir_entry_2 *dentry = NULL;

        path = skip_trailing_backslash(path);
        uint8_t path_len = get_path_token_len(path);

        if (path_len == 0) break;
        inode_get_by_number(inode_idx, &inode);

        inode_dir_ctx_reset(dctx, &inode);
        while ((dentry = inode_dentry_get(&inode, offset, dctx))) {
            offset += dentry->rec_len;

            if (!dentry->inode) continue;
            if (path_len != dentry->name_len) continue;
            if (memcmp(path, dentry->name, dentry->name_len)) continue;

            inode_idx = dentry->inode;
            DEBUG("Lookup following inode %d", inode_idx);

            if (S_ISDIR(inode.i_mode)) {
                dc_entry = dcache_insert(dc_entry, path, path_len, inode_idx);
            }
            break;
        }

        /* Couldn't find the entry at all */
        if (dentry == NULL) {
            inode_idx = 0;
            break;
        }
    } while((path = strchr(path, '/')));

    inode_dir_ctx_put(dctx);
    return inode_idx;
}

int inode_get_by_path(const char *path, struct ext4_inode *inode)
{
    return inode_get_by_number(inode_get_idx_by_path(path), inode);
}

int inode_init(void)
{
    return dcache_init_root(ROOT_INODE_N);
}
