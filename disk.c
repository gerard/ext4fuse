/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include "disk.h"
#include "e4flib.h"
#include "logging.h"

static int disk_fd = -1;


int disk_open(const char *path)
{
    disk_fd = open(path, O_RDONLY);
    if (disk_fd < 0) {
        perror("open");
        return -1;
    }

    return 0;
}

int __disk_read(off_t where, size_t size, void *p, const char *func, int line)
{
    static pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
    ssize_t read_ret;
    off_t lseek_ret;

    ASSERT(disk_fd >= 0);

    pthread_mutex_lock(&read_lock);
    DEBUG("Disk Read: 0x%jx +0x%zx [%s:%d]", where, size, func, line);
    lseek_ret = lseek(disk_fd, where, SEEK_SET);
    read_ret = read(disk_fd, p, size);
    pthread_mutex_unlock(&read_lock);
    if (size == 0) WARNING("Read operation with 0 size");

    ASSERT(lseek_ret == where);
    ASSERT(read_ret == size);

    return read_ret;
}

int disk_ctx_create(struct disk_ctx *ctx, off_t where, size_t size, uint32_t len)
{
    ASSERT(ctx);        /* Should be user allocated */
    ASSERT(size);

    ctx->cur = where;
    ctx->size = size * len;
    DEBUG("New disk context: 0x%jx +0x%jx", ctx->cur, ctx->size);

    return 0;
}

int __disk_ctx_read(struct disk_ctx *ctx, size_t size, void *p, const char *func, int line)
{
    int ret = 0;

    ASSERT(ctx->size);
    if (ctx->size == 0) {
        WARNING("Using a context with no bytes left");
        return ret;
    }

    /* Truncate if there are too many bytes requested */
    if (size > ctx->size) {
        size = ctx->size;
    }

    ret = __disk_read(ctx->cur, size, p, func, line);
    ctx->size -= ret;
    ctx->cur += ret;

    return ret;
}
