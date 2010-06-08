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

#include "dcache.h"
#include "disk.h"
#include "extents.h"
#include "inode.h"
#include "logging.h"
#include "super.h"


#define MAX_INDIRECTED_BLOCK        EXT4_NDIR_BLOCKS + (BLOCK_SIZE / sizeof(uint32_t))
#define ROOT_INODE_N                2
#define IS_PATH_SEPARATOR(__c)      ((__c) == '/')


/* Get pblock for a given inode and lblock.  If extent is not NULL, it will
 * store the length of extent, that is, the number of consecutive pblocks
 * that are also consecutive lblocks (not counting the requested one). */
uint64_t inode_get_data_pblock(struct ext4_inode *inode, uint32_t lblock, uint32_t *extent)
{
    if (extent) *extent = 1;

    if (inode->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_inode_extent *inode_ext = (struct ext4_inode_extent *)&inode->i_block;
        return extent_get_pblock(inode_ext, lblock, extent);
    } else {
        ASSERT(lblock <= BYTES2BLOCKS(inode->i_size_lo));

        if (lblock < EXT4_NDIR_BLOCKS) {
            return inode->i_block[lblock];
        }

        if (lblock < MAX_INDIRECTED_BLOCK) {
            uint32_t indirect_block[BLOCK_SIZE];
            uint32_t indirect_index = lblock - EXT4_NDIR_BLOCKS;

            disk_read_block(EXT4_IND_BLOCK, indirect_block);
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

static void dir_ctx_update(struct ext4_inode *inode, uint32_t lblock, struct inode_dir_ctx *ctx)
{
    uint64_t dir_pblock = inode_get_data_pblock(inode, lblock, NULL);
    disk_read_block(dir_pblock, ctx->buf);
    ctx->lblock = lblock;
}

struct inode_dir_ctx *inode_dir_ctx_get(struct ext4_inode *inode)
{
    struct inode_dir_ctx *ret = malloc(sizeof(struct inode_dir_ctx) + BLOCK_SIZE);
    dir_ctx_update(inode, 0, ret);

    return ret;
}

void inode_dir_ctx_put(struct inode_dir_ctx *ctx)
{
    free(ctx);
}

struct ext4_dir_entry_2 *inode_dentry_get(struct ext4_inode *inode, off_t offset, struct inode_dir_ctx *ctx)
{
    uint32_t lblock = offset / BLOCK_SIZE;
    uint32_t blk_offset = offset % BLOCK_SIZE;

    DEBUG("%d/%d", offset, inode->i_size_lo);
    ASSERT(inode->i_size_lo >= offset);
    if (inode->i_size_lo == offset) {
        return NULL;
    }

    if (lblock == ctx->lblock) {
        return (struct ext4_dir_entry_2 *)&ctx->buf[blk_offset];
    } else {
        dir_ctx_update(inode, lblock, ctx);
        return inode_dentry_get(inode, offset, ctx);
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
    uint32_t inode_idx;
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

        if (path_len == 0) return inode_idx;
        inode_get_by_number(inode_idx, &inode);

        struct inode_dir_ctx *dctx = inode_dir_ctx_get(&inode);
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
        inode_dir_ctx_put(dctx);

        /* Couldn't find the entry at all */
        if (dentry == NULL) return 0;
    } while((path = strchr(path, '/')));

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
