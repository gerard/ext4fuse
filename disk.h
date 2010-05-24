#ifndef DISK_H
#define DISK_H

#include <sys/types.h>
#include "common.h"

#define disk_read(__where, __size, __p)         __disk_read(__where, __size, __p, __func__, __LINE__)
#define disk_read_block(__block, __p)           disk_read_blocks(__block, 1, __p)
#define disk_read_blocks(__blocks, __n, __p)    __disk_read(BLOCKS2BYTES(__blocks), BLOCKS2BYTES(__n), __p, __func__, __LINE__)

int __disk_read(off_t where, size_t size, void *p, const char *func, int line);
int disk_open(const char *path);

#endif
