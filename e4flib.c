/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "disk.h"
#include "e4flib.h"
#include "extents.h"
#include "inode.h"
#include "logging.h"
#include "super.h"

#define IS_PATH_SEPARATOR(__c)      ((__c) == '/')

#define MAX_DIRECTED_BLOCK          12
#define INDIRECT_BLOCK_L1           12
#define MAX_INDIRECTED_BLOCK        MAX_DIRECTED_BLOCK + (BLOCK_SIZE / sizeof(uint32_t))

#define E4F_FREE(__ptr)    ({           \
    free(__ptr);                        \
    (__ptr) = NULL;                     \
})


/* We assume that the data block is a directory */
struct ext4_dir_entry_2 **get_all_directory_entries(uint8_t *blocks, uint32_t size, int *n_read)
{
    /* The smallest directory entry is 12 bytes */
    struct ext4_dir_entry_2 **entry_table = malloc(sizeof(struct ext4_dir_entry_2 *) * (size / 12));
    uint8_t *data_end = blocks + size;
    uint32_t entry_count = 0;

    memset(entry_table, 0, sizeof(struct ext4_dir_entry_2 *) * (size / 12));

    while(blocks < data_end) {
        struct ext4_dir_entry_2 *new_entry = (struct ext4_dir_entry_2 *)blocks;
        blocks += new_entry->rec_len;
        ASSERT(new_entry->rec_len >= 12);

        /* At least the lost+found directories seem to have directory entries
         * with 0-inode.  Skip over them and never report them back. */
        if (new_entry->inode) {
            entry_table[entry_count] = new_entry;
            entry_count++;
        }
    }

    if (n_read) *n_read = entry_count;
    return entry_table;
    /* return realloc(entry_table, sizeof(struct ext4_dir_entry_2 *) * entry_count); */
}

char *e4flib_get_printable_name(char *s, struct ext4_dir_entry_2 *entry)
{
    memcpy(s, entry->name, entry->name_len);
    s[entry->name_len] = 0;
    return s;
}

uint8_t get_path_token_len(const char *path)
{
    uint8_t len = 0;
    while (path[len] != '/' && path[len]) len++;
    return len;
}

uint8_t *e4flib_get_data_blocks_from_inode(struct ext4_inode *inode)
{
    /* NOTE: We cannot use i_blocks and friends, because those count the
     * overhead caused by indirection blocks and extent blocks */
    uint32_t n_blocks = BYTES2BLOCKS(inode->i_size_lo);
    uint8_t *buf = MALLOC_BLOCKS(n_blocks);

    DEBUG("Reading in bunch %d blocks [%d bytes]", n_blocks, inode->i_size_lo);
    for (uint32_t i = 0; i < n_blocks; i++) {
        uint64_t pblock = inode_get_data_pblock(inode, i, NULL);
        ASSERT(pblock != 0);

        disk_read_block(pblock, buf + BLOCKS2BYTES(i));
    }

    return buf;
}

struct ext4_dir_entry_2 **e4flib_get_dentries_inode(struct ext4_inode *inode, int *n_read)
{
    uint8_t *data = e4flib_get_data_blocks_from_inode(inode);

    return get_all_directory_entries(data, inode->i_size_lo, n_read);
}

int e4flib_lookup_path(const char *path, struct ext4_inode **ret_inode)
{
    struct ext4_dir_entry_2 **dir_entries;
    struct ext4_inode *lookup_inode;
    uint8_t *lookup_blocks;
    int n_entries;


    DEBUG("Looking up: %s", path);
    if (!IS_PATH_SEPARATOR(path[0])) {
        return -ENOENT;
    }

    /* FIXME: 2 is the ROOT_INODE.  Add a #define. */
    lookup_inode = inode_get(2);

    do {
        path++; /* Skip over the slash */
        if (!*path) { /* Root inode */
            *ret_inode = lookup_inode;
            return 0;
        }

        uint8_t path_len = get_path_token_len(path);

        lookup_blocks = e4flib_get_data_blocks_from_inode(lookup_inode);
        dir_entries = get_all_directory_entries(lookup_blocks, lookup_inode->i_size_lo, &n_entries);
        inode_put(lookup_inode);

        int i;
        for (i = 0; i < n_entries; i++) {
            char buffer[EXT4_NAME_LEN];
            e4flib_get_printable_name(buffer, dir_entries[i]);

            if (path_len != dir_entries[i]->name_len) continue;

            if (!memcmp(path, dir_entries[i]->name, dir_entries[i]->name_len)) {
                DEBUG("Lookup following inode %d", dir_entries[i]->inode);
                lookup_inode = inode_get(dir_entries[i]->inode);

                break;
            }
        }
        E4F_FREE(lookup_blocks);
        E4F_FREE(dir_entries);

        /* Couldn't find the entry at all */
        if (i == n_entries) return -ENOENT;
    } while((path = strchr(path, '/')));

    if (ret_inode) *ret_inode = lookup_inode;
    else inode_put(lookup_inode);

    return 0;
}

int e4flib_initialize(char *fs_file)
{
    if (disk_open(fs_file) < 0) {
        DEBUG("Couldn't initialize disk");
        return -1;
    }

    return 0;
}
