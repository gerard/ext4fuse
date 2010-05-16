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

struct ext4_super_block *ext4_sb;
struct ext4_group_desc **ext4_gd_table;
struct ext4_inode *root_inode;
int fd;

/* NOTE: We just suppose this runs on LE machines! */

int read_disk(int fd, off_t where, size_t size, void *p)
{
    off_t cur = lseek(fd, 0, SEEK_CUR);
    int ret;

    printf("Disk Read: 0x%08zx +0x%zx\n", where, size);

    lseek(fd, where, SEEK_SET);
    ret = read(fd, p, size);
    lseek(fd, cur, SEEK_SET);

    if (ret == -1) {
        perror("read");
        return ret;
    }

    if (ret != size) {
        fprintf(stderr, "Read returns less than expected [%d/%zd]\n", ret, size);
    }

    return ret;
}

uint32_t get_block_size(void) {
    return 1 << ext4_sb->s_log_block_size + 10;
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

struct ext4_super_block *get_super_block(int fd)
{
    struct ext4_super_block *ret = malloc(sizeof(struct ext4_super_block));

    read_disk(fd, BOOT_SECTOR_SIZE, sizeof(struct ext4_super_block), ret);

    if (ret->s_magic != 0xEF53) return NULL;
    else return ret;
}

struct ext4_group_desc *get_group_descriptor(int fd, int n)
{
    struct ext4_group_desc *ret = malloc(sizeof(struct ext4_group_desc));
    off_t bg_off = BOOT_SECTOR_SIZE + sizeof(struct ext4_super_block) + n * get_group_desc_size();

    read_disk(fd, bg_off, sizeof(struct ext4_group_desc), ret);

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

    read_disk(fd, inode_off, ext4_sb->s_inode_size, ret);

    return ret;
}

/* We assume that the data block is a directory */
struct ext4_dir_entry_2 **get_all_directory_entries(uint32_t n_block, uint32_t size, int *n_read)
{
    /* The smallest directory entry is 12 bytes */
    struct ext4_dir_entry_2 **entry_table = malloc(get_block_size() / 12);
    uint8_t *data_block = malloc(size);

    off_t block_offset = n_block * get_block_size();
    uint8_t *data_end = data_block + size;
    uint32_t entry_count = 0;

    memset(data_block, 0, get_block_size());
    memset(entry_table, 0, sizeof(struct ext4_dir_entry_2 *) * (get_block_size() / 12));

    read_disk(fd, block_offset, size, data_block);
    while(data_block < data_end) {
        entry_table[entry_count] = (struct ext4_dir_entry_2 *)data_block;
        assert(entry_table[entry_count]->rec_len >= 12);
        data_block += entry_table[entry_count]->rec_len;
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

struct ext4_extent *get_extent_from_inode(struct ext4_inode *inode, int n)
{
    return (struct ext4_extent *)(((char *)inode->i_block) + sizeof(struct ext4_extent_header)
                                                           + n * sizeof(struct ext4_extent));
}

int lookup_path(char *path, struct ext4_inode **ret_inode)
{
    struct ext4_dir_entry_2 **dir_entries;
    struct ext4_inode *lookup_inode;
    int n_entries;

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
                assert(extent->ee_start_hi == 0);

                dir_entries = get_all_directory_entries(extent->ee_start_lo, lookup_inode->i_size_lo, &n_entries);
            } else {
                assert(0);          /* Still don't know how to deal with them */
            }
        } else {
            dir_entries = get_all_directory_entries(lookup_inode->i_block[0], lookup_inode->i_size_lo, &n_entries);
        }

        int i;
        for (i = 0; i < n_entries; i++) {
            char buffer[EXT4_NAME_LEN];
            get_printable_dirname(buffer, dir_entries[i]);

            if (path_len != dir_entries[i]->name_len) continue;

            if (!memcmp(path, dir_entries[i]->name, dir_entries[i]->name_len)) {
                printf("Lookup following inode %d\n", dir_entries[i]->inode);
                lookup_inode = get_inode(dir_entries[i]->inode);

                break;
            }
        }

        /* Couldn't find the entry at all */
        if (i == n_entries) return -ENOENT;
    } while((path = strchr(path, '/')));

    *ret_inode = lookup_inode;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "I need a file or device\n");
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    if ((ext4_sb = get_super_block(fd)) == NULL) {
        fprintf(stderr, "No ext4 format found\n");
        return EXIT_FAILURE;
    }

    ext4_gd_table = malloc(sizeof(struct ext4_group_desc *) * get_n_block_groups());
    for (int i = 0; i < get_n_block_groups(); i++) {
        ext4_gd_table[i] = get_group_descriptor(fd, i);
    }

    root_inode = get_inode(2);
    struct ext4_dir_entry_2 **root_entries = get_all_directory_entries(root_inode->i_block[0], root_inode->i_size_lo, NULL);

    for (int i = 0; root_entries[i]; i++) {
        char buffer[256];
        get_printable_dirname(buffer, root_entries[i]);

        printf("/%s\n", buffer);
    }

    struct ext4_inode *test_inode;
    if (lookup_path("/lost+found", &test_inode) == 0) {
        printf("Found.  Permissions: %o\n", test_inode->i_mode);
    }
    if (lookup_path("/.", &test_inode) == 0) {
        printf("Found.  Permissions: %o\n", test_inode->i_mode);
    }
    if (lookup_path("/dir1/dir2/dir3/file", &test_inode) == 0) {
        printf("Found.  Permissions: %o\n", test_inode->i_mode);
    }

    return EXIT_SUCCESS;
}
