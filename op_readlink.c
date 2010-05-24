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

#include "ext4.h"
#include "e4flib.h"
#include "logging.h"
#include "disk.h"


static int get_link_dest(struct ext4_inode *inode, char *buf)
{
    if (inode->i_size_lo <= 60) {
        /* Link destination fits in inode */
        memcpy(buf, inode->i_block, inode->i_size_lo);
    } else {
        uint64_t pblock = e4flib_get_pblock_from_inode(inode, 0);
        disk_read_block(pblock, (uint8_t *)buf);
    }

    buf[inode->i_size_lo] = 0;
    return inode->i_size_lo + 1;
}

/* Check return values, bufer sizes and so on; strings are nasty... */
int e4f_readlink(const char *path, char *buf, size_t bufsize)
{
    struct ext4_inode *inode;
    int ret = 0;

    e4flib_lookup_path(path, &inode);
    if (!S_ISLNK(inode->i_mode)) {
        ret = -EINVAL;
        goto fail;
    }

    get_link_dest(inode, buf);
    DEBUG("Link resolved: %s => %s", path, buf);

fail:
    e4flib_free_inode(inode);
    return ret;
}
