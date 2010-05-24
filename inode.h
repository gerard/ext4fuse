#ifndef INODE_H
#define INODE_H

struct ext4_inode;

struct ext4_inode *inode_get(uint32_t inode_num);
uint64_t inode_get_data_pblock(struct ext4_inode *inode, uint32_t lblock, uint32_t *extent);
void inode_put(struct ext4_inode *inode);

#endif
