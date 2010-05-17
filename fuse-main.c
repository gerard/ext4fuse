/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public Licens
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-
 */

/* Obviously inspired on the great FUSE hello world example:
 *      - http://fuse.sourceforge.net/helloworld.html
 */

#include <fuse.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext4.h"
#include "e4flib.h"


static int e4f_getattr(const char *path, struct stat *stbuf)
{
    struct ext4_inode *inode;
    int ret = 0;

    memset(stbuf, 0, sizeof(struct stat));
    ret = e4flib_lookup_path(path, &inode);

    if (ret < 0) {
        return ret;
    }

    stbuf->st_mode = inode->i_mode;
    stbuf->st_nlink = inode->i_links_count;

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

static int e4f_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
    const char *test_string = "You are testing ext4fuse.  Glitches expected!\n";
    size_t len = strlen(test_string);

    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, test_string + offset, size);
    } else {
        size = 0;
    }

    return size;
}

static struct fuse_operations e4f_ops = {
    .getattr    = e4f_getattr,
    .readdir    = e4f_readdir,
    .open       = e4f_open,
    .read       = e4f_read,
};

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("I need one filesystem to mount\n");
    }

    if (e4flib_initialize(argv[1]) < 0) {
        printf("Failed to initialize ext4fuse\n");
    }

    argc--;
    argv[1] = argv[2];
    argv[2] = NULL;

    return fuse_main(argc, argv, &e4f_ops);
}   
