#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ext4.h"
#include "super.h"
#include "disk.h"

struct ext4_inode *inode_get(uint32_t inode_num)
{
    if (inode_num == 0) return NULL;
    inode_num--;    /* Inode 0 doesn't exist on disk */

    struct ext4_inode *ret = malloc(super_inode_size());
    memset(ret, 0, super_inode_size());

    off_t inode_off = super_group_inode_table_offset(inode_num);
    inode_off += (inode_num % super_inodes_per_group()) * super_inode_size();

    disk_read(inode_off, super_inode_size(), ret);
    return ret;
}

void inode_put(struct ext4_inode *inode)
{
    free(inode);
}
