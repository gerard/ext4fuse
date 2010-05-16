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
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "ext4.h"

#define BOOT_SECTOR_SIZE            0x400
#define GROUP_DESC_MIN_SIZE         0x20
#define IS_PATH_SEPARATOR(__c)      ((__c) == '/')

#define E4F_DEBUG(format, ...)      fprintf(stderr, "[%s:%d] " format "\n"  \
                                                  , __PRETTY_FUNCTION__     \
                                                  , __LINE__, ##__VA_ARGS__)

struct ext4_super_block *ext4_sb;
struct ext4_group_desc **ext4_gd_table;
struct ext4_inode *root_inode;
int fd;

/* NOTE: We just suppose this runs on LE machines! */

#define read_disk(__where, __size, __p)     __read_disk(__where, __size, __p, __func__, __LINE__)
#define read_disk_block(__block, __p)       __read_disk((__block) * get_block_size(), get_block_size(), __p, __func__, __LINE__)

#define E4F_FREE(__ptr)    ({           \
    free(__ptr);                        \
    (__ptr) = NULL;                     \
})

uint32_t get_block_size(void) {
    return 1 << ext4_sb->s_log_block_size + 10;
}

void *malloc_blocks(size_t n)
{
    return malloc(n * get_block_size());
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
    return get_block_size() * ext4_sb->s_blocks_per_group;
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

struct ext4_super_block *get_super_block()
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
    off_t inode_off = gdesc->bg_inode_table_lo * get_block_size()
                    + (inode_num % ext4_sb->s_inodes_per_group) * ext4_sb->s_inode_size;

    read_disk(inode_off, ext4_sb->s_inode_size, ret);

    return ret;
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
        assert(entry_table[entry_count]->rec_len >= 12);
        blocks += entry_table[entry_count]->rec_len;
        entry_count++;
    }

    if (n_read) *n_read = entry_count;
    return entry_table;
    /* return realloc(entry_table, sizeof(struct ext4_dir_entry_2 *) * entry_count); */
}

char *get_printable_dirname(char *s, struct ext4_dir_entry_2 *entry)
{
    memcpy(s, entry->name, entry->name_len);
    s[entry->name_len] = 0;
    return s;
}

uint8_t get_path_token_len(char *path)
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

struct ext4_extent *get_extent_from_leaf(uint32_t leaf_block, int *n_entries)
{
    struct ext4_extent_header ext_h;
    struct ext4_extent *exts;

    read_disk(leaf_block * get_block_size(), sizeof(struct ext4_extent_header), &ext_h);
    assert(ext_h.eh_depth == 0);

    uint32_t extents_length = ext_h.eh_entries * sizeof(struct ext4_extent);
    exts = malloc(extents_length);

    uint32_t where = leaf_block * get_block_size() + sizeof(struct ext4_extent);
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

int lookup_path(char *path, struct ext4_inode **ret_inode)
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

        /* FIXME: We only check first block, assuming the dir entry is not too big.
         *        i_blocks_count_lo can be used */
        if (lookup_inode->i_flags & EXT4_EXTENTS_FL) {
            struct ext4_extent_header *ext_header = get_extent_header_from_inode(lookup_inode);

            assert(ext_header->eh_magic == EXT4_EXT_MAGIC);
            if (ext_header->eh_depth == 0) {
                /* These assertions are not real, of course.  These parameters
                 * could be almost anything.  We are trying to handle the easy
                 * case for now. */
                assert(ext_header->eh_entries == 1);
                struct ext4_extent *extent = get_extent_from_inode(lookup_inode, 0);

                assert(extent->ee_block == 0);
                assert(extent->ee_len == 1);
                assert(lookup_inode->i_size_lo <= get_block_size());
                assert(extent->ee_start_hi == 0);

                lookup_blocks = malloc_blocks(1);
                read_disk_block(extent->ee_start_lo, lookup_blocks);
                dir_entries = get_all_directory_entries(lookup_blocks, lookup_inode->i_size_lo, &n_entries);
            } else {
                assert(ext_header->eh_entries == 1);
                assert(ext_header->eh_depth == 1);

                int n_extents;
                struct ext4_extent_idx *ext_idx = get_extent_idx_from_inode(lookup_inode, 0);
                struct ext4_extent *extents = get_extent_from_leaf(ext_idx->ei_leaf_lo, &n_extents);
                uint8_t dir_blocks[get_block_size() * n_extents];

                assert(n_extents);
                lookup_blocks = malloc_blocks(get_blocks_in_extents(extents, n_extents));
                int cur_block = 0;
                E4F_DEBUG("%x", lookup_inode->i_size_lo);

                for (int i = 0; i < n_extents; i++) {
                    assert(extents[i].ee_start_hi == 0);
                    assert(cur_block == extents[i].ee_block);

                    E4F_DEBUG("Length: %d | LBlock: %d", extents[i].ee_len, extents[i].ee_block);
                    for (int j = 0; j < extents[i].ee_len; j++) {
                        read_disk_block(extents[i].ee_start_lo + j,
                                        lookup_blocks + cur_block * get_block_size());
                        cur_block++;
                    }
                }

                dir_entries = get_all_directory_entries(lookup_blocks, lookup_inode->i_size_lo, &n_entries);
            }
        } else {
            assert(lookup_inode->i_size_lo <= get_block_size());

            lookup_blocks = malloc_blocks(1);
            read_disk_block(lookup_inode->i_block[0], lookup_blocks);
            dir_entries = get_all_directory_entries(lookup_blocks, lookup_inode->i_size_lo, &n_entries);
        }

        int i;
        for (i = 0; i < n_entries; i++) {
            char buffer[EXT4_NAME_LEN];
            get_printable_dirname(buffer, dir_entries[i]);

            if (path_len != dir_entries[i]->name_len) continue;

            E4F_DEBUG("CMP %s <=> %s", path, dir_entries[i]->name);
            if (!memcmp(path, dir_entries[i]->name, dir_entries[i]->name_len)) {
                E4F_DEBUG("Lookup following inode %d", dir_entries[i]->inode);
                lookup_inode = get_inode(dir_entries[i]->inode);

                break;
            }
        }
        E4F_FREE(lookup_blocks);

        /* Couldn't find the entry at all */
        if (i == n_entries) return -ENOENT;
    } while((path = strchr(path, '/')));

    *ret_inode = lookup_inode;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        E4F_DEBUG("I need a file or device");
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    if ((ext4_sb = get_super_block(fd)) == NULL) {
        E4F_DEBUG("No ext4 format found\n");
        return EXIT_FAILURE;
    }

    ext4_gd_table = malloc(sizeof(struct ext4_group_desc *) * get_n_block_groups());
    for (int i = 0; i < get_n_block_groups(); i++) {
        ext4_gd_table[i] = get_group_descriptor(i);
    }

    root_inode = get_inode(2);

    struct ext4_inode *test_inode;
    assert(lookup_path("/lost+found", &test_inode) == 0);
    assert(lookup_path("/.", &test_inode) == 0);
    assert(lookup_path("/dir1/dir2/dir3/file", &test_inode) == 0);
    assert(lookup_path("/Documentation/mips/00-INDEX", &test_inode) == 0);
    E4F_DEBUG("Permissions: %o\n", test_inode->i_mode);

    return EXIT_SUCCESS;
}
