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


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "ext4.h"
#include "e4flib.h"

#define BOOT_SECTOR_SIZE            0x400
#define GROUP_DESC_MIN_SIZE         0x20
#define IS_PATH_SEPARATOR(__c)      ((__c) == '/')

#define MAX_DIRECTED_BLOCK          12
#define INDIRECT_BLOCK_L1           12
#define MAX_INDIRECTED_BLOCK        MAX_DIRECTED_BLOCK + (get_block_size() / sizeof(uint32_t))

struct ext4_super_block *ext4_sb;
struct ext4_group_desc **ext4_gd_table;
struct ext4_inode *root_inode;
int fd;
FILE *logfile_fd = NULL;

/* NOTE: We just suppose this runs on LE machines! */

#define read_disk(__where, __size, __p)         __read_disk(__where, __size, __p, __func__, __LINE__)

#define read_disk_block(__block, __p)           read_disk_blocks(__block, 1, __p)
#define read_disk_blocks(__blocks, __n, __p)    __read_disk(BLOCKS2BYTES(__blocks), BLOCKS2BYTES(__n), __p, __func__, __LINE__)

#define E4F_FREE(__ptr)    ({           \
    free(__ptr);                        \
    (__ptr) = NULL;                     \
})


uint32_t get_block_size(void) {
    return 1 << ext4_sb->s_log_block_size + 10;
}

int __read_disk(off_t where, size_t size, void *p, const char *func, int line)
{
    off_t cur = lseek(fd, 0, SEEK_CUR);
    int ret;

    E4F_DEBUG("Disk Read: 0x%08zx +0x%zx [%s:%d]", where, size, func, line);

    lseek(fd, where, SEEK_SET);
    ret = read(fd, p, size);
    lseek(fd, cur, SEEK_SET);

    if (ret == -1) {
        perror("read");
        return ret;
    }

    if (ret != size) {
        E4F_DEBUG("Read returns less than expected [%d/%zd]", ret, size);
    }

    return ret;
}

uint32_t get_block_group_size(void)
{
    return BLOCKS2BYTES(ext4_sb->s_blocks_per_group);
}

uint32_t get_n_block_groups(void)
{
    return ext4_sb->s_blocks_count_lo / ext4_sb->s_blocks_per_group;
}

uint32_t get_group_desc_size(void)
{
    if (!ext4_sb->s_desc_size) return GROUP_DESC_MIN_SIZE;
    else return sizeof(struct ext4_group_desc);
}

struct ext4_super_block *get_super_block(void)
{
    struct ext4_super_block *ret = malloc(sizeof(struct ext4_super_block));

    read_disk(BOOT_SECTOR_SIZE, sizeof(struct ext4_super_block), ret);

    if (ret->s_magic != 0xEF53) return NULL;
    else return ret;
}

struct ext4_group_desc *get_group_descriptor(int n)
{
    struct ext4_group_desc *ret = malloc(sizeof(struct ext4_group_desc));
    off_t bg_off = BOOT_SECTOR_SIZE + sizeof(struct ext4_super_block) + n * get_group_desc_size();

    read_disk(bg_off, sizeof(struct ext4_group_desc), ret);

    return ret;
}

uint32_t get_block_group_for_inode(uint32_t inode_num)
{
    return inode_num / ext4_sb->s_inodes_per_group;
}

struct ext4_inode *get_inode(uint32_t inode_num)
{
    if (inode_num == 0) return NULL;
    inode_num--;    /* Inode 0 doesn't exist on disk */

    /* We might not read the whole struct if disk inodes are smaller */
    struct ext4_inode *ret = malloc(sizeof(struct ext4_inode));
    memset(ret, 0, sizeof(struct ext4_inode));

    struct ext4_group_desc *gdesc = ext4_gd_table[get_block_group_for_inode(inode_num)];
    off_t inode_off = BLOCKS2BYTES(gdesc->bg_inode_table_lo)
                    + (inode_num % ext4_sb->s_inodes_per_group) * ext4_sb->s_inode_size;

    read_disk(inode_off, ext4_sb->s_inode_size, ret);

    return ret;
}

void e4flib_free_inode(struct ext4_inode *inode)
{
    if (inode == root_inode) return;
    E4F_FREE(inode);
}


/* We assume that the data block is a directory */
struct ext4_dir_entry_2 **get_all_directory_entries(uint8_t *blocks, uint32_t size, int *n_read)
{
    /* The smallest directory entry is 12 bytes */
    struct ext4_dir_entry_2 **entry_table = malloc(sizeof(struct ext4_dir_entry_2 *) * (size / 12));
    uint8_t *data_end = blocks + size;
    uint32_t entry_count = 0;

    memset(entry_table, 0, sizeof(struct ext4_dir_entry_2 *) * (size / 12));

    while(blocks < data_end) {
        entry_table[entry_count] = (struct ext4_dir_entry_2 *)blocks;
        E4F_ASSERT(entry_table[entry_count]->rec_len >= 12);
        blocks += entry_table[entry_count]->rec_len;
        entry_count++;
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

struct ext4_extent_header *get_extent_header_from_inode(struct ext4_inode *inode)
{
    return (struct ext4_extent_header *)inode->i_block;
}

struct ext4_extent_idx *get_extent_idx_from_inode(struct ext4_inode *inode, int n)
{
    return (struct ext4_extent_idx *)(((char *)inode->i_block) + sizeof(struct ext4_extent_header)
                                                               + n * sizeof(struct ext4_extent_idx));
}

struct ext4_extent *get_extent_from_inode(struct ext4_inode *inode, int n)
{
    return (struct ext4_extent *)(((char *)inode->i_block) + sizeof(struct ext4_extent_header)
                                                           + n * sizeof(struct ext4_extent));
}

struct ext4_extent *get_extents_from_ext_header(struct ext4_extent_header *h)
{
    return (struct ext4_extent *)(((char *)h) + sizeof(struct ext4_extent_header));
}

struct ext4_extent_idx *get_extent_idxs_from_ext_header(struct ext4_extent_header *h)
{
    return (struct ext4_extent_idx *)(((char *)h) + sizeof(struct ext4_extent_header));
}

struct ext4_extent *get_extent_from_leaf(uint32_t leaf_block, int *n_entries)
{
    struct ext4_extent_header ext_h;
    struct ext4_extent *exts;

    read_disk(BLOCKS2BYTES(leaf_block), sizeof(struct ext4_extent_header), &ext_h);
    E4F_ASSERT(ext_h.eh_depth == 0);

    uint32_t extents_length = ext_h.eh_entries * sizeof(struct ext4_extent);
    exts = malloc(extents_length);

    uint32_t where = BLOCKS2BYTES(leaf_block) + sizeof(struct ext4_extent);
    read_disk(where, extents_length, exts);

    if (n_entries) *n_entries = ext_h.eh_entries;
    return exts;
}

uint32_t get_blocks_in_extents(struct ext4_extent *exts, int n)
{
    uint32_t ret = 0;

    for (int i = 0; i < n; i++) {
        ret += exts[i].ee_len;
    }

    return ret;
}

uint8_t *e4flib_get_data_blocks_from_inode(struct ext4_inode *inode)
{
    uint8_t *blocks = NULL;

    if (inode->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_extent_header *ext_header = get_extent_header_from_inode(inode);
        struct ext4_extent *extents = NULL;

        E4F_ASSERT(ext_header->eh_magic == EXT4_EXT_MAGIC);

        if (ext_header->eh_depth == 0) {
            /* These assertions are not real, of course.  These parameters
             * could be almost anything.  We are trying to handle the easy
             * case for now. */
            int cur_block = 0;

            extents = get_extent_from_inode(inode, 0);
            blocks = malloc_blocks(get_blocks_in_extents(extents, ext_header->eh_entries));
            
            E4F_ASSERT(ext_header->eh_entries <= 6);

            /* Factor this loop out (same than the next case) */
            for (int i = 0; i < ext_header->eh_entries; i++) {
                E4F_ASSERT(extents[i].ee_start_hi == 0);
                E4F_ASSERT(cur_block == extents[i].ee_block);

                E4F_DEBUG("Length: %d | LBlock: %d", extents[i].ee_len, extents[i].ee_block);
                read_disk_blocks(extents[i].ee_start_lo, extents[i].ee_len, blocks + BLOCKS2BYTES(cur_block));
                cur_block += extents[i].ee_len;
            }
        } else {
            int n_extents;
            struct ext4_extent_idx *ext_idx = get_extent_idx_from_inode(inode, 0);
            int cur_block = 0;

            extents = get_extent_from_leaf(ext_idx->ei_leaf_lo, &n_extents);
            blocks = malloc_blocks(get_blocks_in_extents(extents, n_extents));

            E4F_ASSERT(ext_header->eh_entries == 1);
            E4F_ASSERT(ext_header->eh_depth == 1);
            E4F_ASSERT(n_extents);

            E4F_DEBUG("%x", inode->i_size_lo);

            for (int i = 0; i < n_extents; i++) {
                E4F_ASSERT(extents[i].ee_start_hi == 0);
                E4F_ASSERT(cur_block == extents[i].ee_block);

                E4F_DEBUG("Length: %d | LBlock: %d", extents[i].ee_len, extents[i].ee_block);
                read_disk_blocks(extents[i].ee_start_lo, extents[i].ee_len, blocks + BLOCKS2BYTES(cur_block));
                cur_block += extents[i].ee_len;
            }

            /* Only free extents if they weren't part of the inode! */
            E4F_FREE(extents);
        }
    } else {
        E4F_ASSERT(inode->i_size_lo <= get_block_size());

        blocks = malloc_blocks(1);
        read_disk_block(inode->i_block[0], blocks);
    }

    E4F_ASSERT(blocks);
    return blocks;
}

int get_block_from_extents(struct ext4_extent *ee, uint32_t n_entries, uint32_t n_block, uint8_t *block)
{
    int block_ext_index = 0;
    int block_ext_offset = 0;
    int i;

    E4F_DEBUG("Extent contains %d entries", n_entries);

    /* Skip to the right extent entry */
    for (i = 0; i < n_entries; i++) {
        E4F_ASSERT(ee[i].ee_start_hi == 0);
        E4F_DEBUG("Want: %d | Length: %d | LBlock: %d", n_block, ee[i].ee_len, ee[i].ee_block);

        if (ee[i].ee_block + ee[i].ee_len >= n_block) {
            E4F_DEBUG("Block located");
            block_ext_index = i;
            block_ext_offset = n_block - ee[i].ee_block;
            break;
        }
    }
    E4F_ASSERT(i != n_entries);

    read_disk_block(ee[block_ext_index].ee_start_lo + block_ext_offset, block);
    return 0;
}

int get_block_from_extent_header(struct ext4_extent_header *eh, uint32_t n, uint8_t *block)
{
    E4F_ASSERT(eh->eh_magic == EXT4_EXT_MAGIC);
    E4F_ASSERT(eh->eh_entries <= 6);

    if (eh->eh_depth == 0) {
        struct ext4_extent *extents = get_extents_from_ext_header(eh);

        return get_block_from_extents(extents, eh->eh_entries, n, block);
    } else {
        /* Not valid assertions, but we can deal with those later.  I really
         * should have look how extent indexes work... */
        E4F_ASSERT(eh->eh_depth == 1);
        E4F_ASSERT(eh->eh_entries == 1);

        int n_leaf_entries;
        struct ext4_extent_idx *ei = get_extent_idxs_from_ext_header(eh);
        struct ext4_extent *ee = get_extent_from_leaf(ei->ei_leaf_lo, &n_leaf_entries);

        return get_block_from_extents(ee, n_leaf_entries, n, block);
    }
}

int e4flib_get_block_from_inode(struct ext4_inode *inode, uint8_t *block, uint32_t n)
{
    E4F_ASSERT(BLOCKS2BYTES(inode->i_blocks_lo) >= inode->i_size_lo);

    if (inode->i_flags & EXT4_EXTENTS_FL) {
        struct ext4_extent_header *ext_header = get_extent_header_from_inode(inode);
        return get_block_from_extent_header(ext_header, n, block);
    } else {
        if (n >= inode->i_blocks_lo) {
            E4F_DEBUG("Block to big for this file [%d/%d]", n, inode->i_blocks_lo);
            return -1;
        }

        if (n < MAX_DIRECTED_BLOCK) {
            read_disk_block(inode->i_block[n], block);
            return 0;
        }

        if (n < MAX_INDIRECTED_BLOCK) {
            uint32_t indirect_block[get_block_size()];
            uint32_t indirect_index = n - MAX_DIRECTED_BLOCK;

            read_disk_block(INDIRECT_BLOCK_L1, indirect_block);
            read_disk_block(indirect_block[indirect_index], block);
            return 0;
        }

        /* Handle this later */
        E4F_ASSERT(n < MAX_INDIRECTED_BLOCK);
        return -1;
    }
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


    E4F_DEBUG("Looking up: %s", path);
    if (!IS_PATH_SEPARATOR(path[0])) {
        return -ENOENT;
    }

    lookup_inode = root_inode;

    do {
        path++; /* Skip over the slash */
        if (!*path) { /* Root inode */
            *ret_inode = root_inode;
            return 0;
        }

        uint8_t path_len = get_path_token_len(path);

        lookup_blocks = e4flib_get_data_blocks_from_inode(lookup_inode);
        dir_entries = get_all_directory_entries(lookup_blocks, lookup_inode->i_size_lo, &n_entries);
        e4flib_free_inode(lookup_inode);

        int i;
        for (i = 0; i < n_entries; i++) {
            char buffer[EXT4_NAME_LEN];
            e4flib_get_printable_name(buffer, dir_entries[i]);

            if (path_len != dir_entries[i]->name_len) continue;

            if (!memcmp(path, dir_entries[i]->name, dir_entries[i]->name_len)) {
                E4F_DEBUG("Lookup following inode %d", dir_entries[i]->inode);
                lookup_inode = get_inode(dir_entries[i]->inode);

                break;
            }
        }
        E4F_FREE(lookup_blocks);
        E4F_FREE(dir_entries);

        /* Couldn't find the entry at all */
        if (i == n_entries) return -ENOENT;
    } while((path = strchr(path, '/')));

    if (ret_inode) *ret_inode = lookup_inode;
    else e4flib_free_inode(lookup_inode);

    return 0;
}

int e4flib_initialize(char *fs_file)
{
    logfile_fd = stderr;
    fd = open(fs_file, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    if ((ext4_sb = get_super_block()) == NULL) {
        E4F_DEBUG("No ext4 format found");
        return -1;
    }

    ext4_gd_table = malloc(sizeof(struct ext4_group_desc *) * get_n_block_groups());
    for (int i = 0; i < get_n_block_groups(); i++) {
        ext4_gd_table[i] = get_group_descriptor(i);
    }

    root_inode = get_inode(2);

    return 0;
}

int e4flib_logfile(const char *logfile)
{
    FILE *lf_fd = fopen(logfile, "w");
    if (lf_fd == NULL) {
        perror("open");
        return -1;
    }

    setbuf(lf_fd, NULL);
    logfile_fd = lf_fd;
    return 0;
}
