/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
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

#include "common.h"
#include "disk.h"
#include "e4flib.h"
#include "inode.h"
#include "logging.h"
#include "ops.h"
#include "super.h"

void signal_handle_sigsegv(int signal)
{
    UNUSED(signal);
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

    inode_put(inode);

    return 0;
}

static int e4f_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
    UNUSED(fi);
    UNUSED(offset);
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
    inode_put(inode);

    return 0;
}

static int e4f_open(const char *path, struct fuse_file_info *fi)
{
    UNUSED(path);
    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;
    return 0;
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
