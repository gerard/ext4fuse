/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.

 * Obviously inspired on the great FUSE hello world example:
 *      - http://fuse.sourceforge.net/helloworld.html
 */


#include <fuse.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>

#include "ext4.h"
#include "e4flib.h"
#include "ops.h"
#include "logging.h"


void signal_handle_sigsegv(int signal)
{
    void *array[10];
    size_t size;
    char **strings;
    size_t i;

    DEBUG("========================================");
    DEBUG("Segmentation Fault.  Starting backtrace:");
    size = backtrace(array, 10);
    strings = backtrace_symbols(array, size);

    for (i = 0; i < size; i++)
        DEBUG("%s", strings[i]);
    DEBUG("========================================");

    abort();
}

static int e4f_getattr(const char *path, struct stat *stbuf)
{
    struct ext4_inode *inode;
    int ret = 0;

    DEBUG("getattr(%s)", path);

    memset(stbuf, 0, sizeof(struct stat));
    ret = e4flib_lookup_path(path, &inode);

    if (ret < 0) {
        return ret;
    }

    ASSERT(inode);

    stbuf->st_mode = inode->i_mode & ~0222;
    stbuf->st_nlink = inode->i_links_count;
    stbuf->st_size = inode->i_size_lo;
    stbuf->st_uid = inode->i_uid;
    stbuf->st_gid = inode->i_gid;
    stbuf->st_atime = inode->i_atime;
    stbuf->st_mtime = inode->i_mtime;
    stbuf->st_ctime = inode->i_ctime;

    e4flib_free_inode(inode);

    return 0;
}

static int e4f_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    struct ext4_inode *inode;
    struct ext4_dir_entry_2 **entries;
    int n_entries = 0;
    int ret = 0;

    ret = e4flib_lookup_path(path, &inode);
    if (ret < 0) {
        return ret;
    }

    entries = e4flib_get_dentries_inode(inode, &n_entries);
    for (int i = 0; i < n_entries; i++) {
        char name_buffer[EXT4_NAME_LEN];
        e4flib_get_printable_name(name_buffer, entries[i]);
        filler(buf, name_buffer, NULL, 0);
    }

    /* Nasty, but works... This points to the allocated data blocks */
    free(entries[0]);
    free(entries);
    e4flib_free_inode(inode);

    return 0;
}

static int e4f_open(const char *path, struct fuse_file_info *fi)
{
    //return e4flib_lookup_path(path, NULL);
    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;
    return 0;
}

/* TODO: All this code is really messy with the edge cases (when a size falls
 *       on modulo block size.  Clean it up at some point. */
static int e4f_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    struct ext4_inode *inode;
    uint8_t *block;
    uint32_t n_block_start, n_block_end;
    off_t block_start_offset;
    size_t first_size;
    int ret;

    DEBUG("read(%s, buf, %jd, %zd, fi)", path, size, offset);

    if ((ret = e4flib_lookup_path(path, &inode))) {
        return ret;
    }

    if (size == 0) {
        return size;
    }

    if (offset >= inode->i_size_lo) {
        return 0;
    }

    if ((offset + size) > inode->i_size_lo) {
        size = inode->i_size_lo - offset;
    }

    n_block_start = offset / get_block_size();
    n_block_end = (offset + (size - 1)) / get_block_size();
    block_start_offset = offset % get_block_size();

    /* If the read request spans over several blocks, we will just return the
     * data contained in the first one */
    if (n_block_start != n_block_end) {
        first_size = ((n_block_start + 1) * get_block_size()) - offset;
        ASSERT(size >= first_size);
    } else {
        first_size = size;
    }


    block = malloc_blocks(1);

    /* First block, might be missaligned */
    e4flib_get_block_from_inode(inode, block, n_block_start);
    DEBUG("read(2): Initial chunk: %jx [%i:%jd] +%zd bytes from %s\n", offset, n_block_start, block_start_offset, first_size, path);
    memcpy(buf, block + block_start_offset, first_size);
    buf += first_size;

    for (int i = n_block_start + 1; i <= n_block_end; i++) {
        e4flib_get_block_from_inode(inode, block, i);

        if (i == n_block_end) {
            DEBUG("read(2): End chunk");
            if ((offset + size) % get_block_size() == 0) {
                memcpy(buf, block, get_block_size());
            } else {
                memcpy(buf, block, (offset + size) % get_block_size());
            }
            /* No need to increase the buffer pointer */
        } else {
            DEBUG("read(2): Middle chunk");
            memcpy(buf, block, get_block_size());
            buf += get_block_size();
        }
    }

    e4flib_free_inode(inode);
    free(block);

    return size;
}

static struct fuse_operations e4f_ops = {
    .getattr    = e4f_getattr,
    .readdir    = e4f_readdir,
    .open       = e4f_open,
    .read       = e4f_read,
    .readlink   = e4f_readlink,
    .init       = op_init,
};

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s fs mountpoint\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGSEGV, signal_handle_sigsegv) == SIG_ERR) {
        fprintf(stderr, "Failed to initialize signals\n");
        return EXIT_FAILURE;
    }

    if (logging_open(argc == 4 ? argv[3] : DEFAULT_LOG_FILE) < 0) {
        fprintf(stderr, "Failed to initialize logging\n");
        return EXIT_FAILURE;
    }

    if (e4flib_initialize(argv[1]) < 0) {
        fprintf(stderr, "Failed to initialize ext4fuse\n");
        return EXIT_FAILURE;
    }

    argc = 2;
    argv[1] = argv[2];
    argv[2] = NULL;

    return fuse_main(argc, argv, &e4f_ops, NULL);
}   
