/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>

#include "common.h"
#include "disk.h"
#include "inode.h"
#include "logging.h"
#include "ops.h"
#include "super.h"


static int get_link_dest(struct ext4_inode *inode, char *buf, size_t bufsize)
{
    uint64_t inode_size = inode_get_size(inode);

    if (inode_size <= 60) {
        /* Link destination fits in inode */
        memcpy(buf, inode->i_block, inode_size);
    } else {
        uint64_t pblock = inode_get_data_pblock(inode, 0, NULL);
        char *block_data = malloc(super_block_size());
        disk_read_block(pblock, (uint8_t *)block_data);
        strncpy(buf, block_data, bufsize - 1);
        free(block_data);
    }

    buf[inode_size] = 0;
    return inode_size + 1;
}

/* Check return values, bufer sizes and so on; strings are nasty... */
int op_readlink(const char *path, char *buf, size_t bufsize)
{
    struct ext4_inode inode;
    DEBUG("readlink");

    inode_get_by_path(path, &inode);
    if (!S_ISLNK(inode.i_mode)) {
        return -EINVAL;
    }

    get_link_dest(&inode, buf, bufsize);
    DEBUG("Link resolved: %s => %s", path, buf);
    return 0;
}
