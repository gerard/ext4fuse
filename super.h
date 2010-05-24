#ifndef SUPER_H
#define SUPER_H

#include <stdint.h>
#include <sys/types.h>

/* struct ext4_super */
uint32_t super_block_size(void);
uint32_t super_inodes_per_group(void);
uint32_t super_inode_size(void);
int super_fill(void);

/* struct ext4_group_desc */
off_t super_group_inode_table_offset(uint32_t inode_num);
int super_group_fill(void);

#endif
