#ifndef INODE_H
#define INODE_H

struct ext4_inode;

struct ext4_inode *inode_get(uint32_t inode_num);
void inode_put(struct ext4_inode *inode);

#endif
