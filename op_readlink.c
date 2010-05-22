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

#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>

#include "ext4.h"
#include "e4flib.h"

#define MIN(x, y)   ({                  \
    typeof (x) __x = (x);               \
    typeof (y) __y = (y);               \
    __x < __y ? __x : __y;              \
})


static int get_link_dest(struct ext4_inode *inode, char *buf)
{
    if (inode->i_size_lo <= 60) {
        /* Link destination fits in inode */
        memcpy(buf, inode->i_block, inode->i_size_lo);
    } else {
        e4flib_get_block_from_inode(inode, (uint8_t *)buf, inode->i_block[0]);
    }

    buf[inode->i_size_lo] = 0;
    return inode->i_size_lo + 1;
}

static int is_absolute_path(const char *path)
{
    return path[0] == '/';
}

/* Check return values, bufer sizes and so on; strings are nasty... */
int e4f_readlink(const char *path, char *buf, size_t bufsize)
{
    struct ext4_inode *inode;
    char link_buf[4096];
    char path_buf[4096];

    e4flib_lookup_path(path, &inode);
    if (!S_ISLNK(inode->i_mode)) {
        e4flib_free_inode(inode);
        return -EINVAL;
    }

    strcpy(path_buf, path);
    dirname(path_buf);

    do {
        get_link_dest(inode, link_buf);
        e4flib_free_inode(inode);

        if (is_absolute_path(link_buf)) {
            strcpy(path_buf, link_buf);
        } else {
            char *path_buf_last = &path_buf[strlen(path_buf) - 1];
            if (*path_buf_last != '/') strcat(path_buf, "/");
            strcat(path_buf, link_buf);
        }

        e4flib_lookup_path(path_buf, &inode);
    } while (S_ISLNK(inode->i_mode & S_IFLNK));
    e4flib_free_inode(inode);

    E4F_DEBUG("Link resolved \"%s\" => \"%s\"", path, path_buf);

    strncpy(buf, path_buf, bufsize);
    return 0;
}
