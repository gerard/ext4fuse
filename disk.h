#ifndef DISK_H
#define DISK_H

#include <sys/types.h>

#include "super.h"

#define disk_read(__where, __s, __p)        __disk_read(__where, __s, __p, __func__, __LINE__)
#define disk_read_block(__blocks, __p)      __disk_read(BLOCKS2BYTES(__blocks), BLOCK_SIZE, __p, __func__, __LINE__)
#define disk_ctx_read(__ctx, __s, __p)      __disk_ctx_read(__ctx, __s, __p, __func__, __LINE__)
#define disk_read_type(__where, __t)        ({                                          \
    __t ret;                                                                            \
    ASSERT(__disk_read(__where, sizeof(__t), &ret, __func__, __LINE__) == sizeof(__t)); \
    ret;                                                                                \
})

#define disk_read_u32(__where)              disk_read_type(__where, uint32_t)

struct disk_ctx {
    off_t cur;              /* Current offset */
    size_t size;            /* How much to read */
};

int disk_open(const char *path);
int __disk_read(off_t where, size_t size, void *p, const char *func, int line);

int disk_ctx_create(struct disk_ctx *ctx, off_t where, size_t size, uint32_t len);
int __disk_ctx_read(struct disk_ctx *ctx, size_t size, void *p, const char *func, int line);


#endif
