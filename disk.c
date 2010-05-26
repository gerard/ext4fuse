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

#include "e4flib.h"
#include "logging.h"

static int disk_fd = -1;

int __disk_read(off_t where, size_t size, void *p, const char *func, int line)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    ssize_t read_ret;
    off_t lseek_ret;

    ASSERT(disk_fd >= 0);

    pthread_mutex_lock(&lock);
    DEBUG("Disk Read: 0x%jx +0x%zx [%s:%d]", where, size, func, line);
    lseek_ret = lseek(disk_fd, where, SEEK_SET);
    read_ret = read(disk_fd, p, size);
    pthread_mutex_unlock(&lock);

    ASSERT(lseek_ret == where);
    ASSERT(read_ret == size);

    return 0;
}

int disk_open(const char *path)
{
    disk_fd = open(path, O_RDONLY);
    if (disk_fd < 0) {
        perror("open");
        return -1;
    }

    return 0;
}
