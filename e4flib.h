#ifndef E4FLIB_H
#define E4FLIB_H

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

struct ext4_inode;
struct ext4_dir_entry_2;

int e4flib_lookup_path(const char *path, struct ext4_inode **ret_inode);
void e4flib_free_inode(struct ext4_inode *inode);
int e4flib_initialize(char *fs_file);
char *e4flib_get_printable_name(char *s, struct ext4_dir_entry_2 *entry);
struct ext4_dir_entry_2 **e4flib_get_dentries_inode(struct ext4_inode *inode, int *n_read);
int e4flib_logfile(const char *logfile);
uint8_t *e4flib_get_data_blocks_from_inode(struct ext4_inode *inode);
uint32_t get_block_size(void);
int e4flib_get_block_from_inode(struct ext4_inode *inode, uint8_t *block, uint32_t n);

#endif
