#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdlib.h>

#include "super.h"


#define __le64      uint64_t
#define __le32      uint32_t
#define __le16      uint16_t
#define __u64       uint64_t
#define __u32       uint32_t
#define __u16       uint16_t
#define __u8        uint8_t


#define BLOCK_SIZE                              (super_block_size())
#define BLOCKS2BYTES(__blks)                    (((uint64_t)(__blks)) * BLOCK_SIZE)
#define BYTES2BLOCKS(__bytes)                   ((__bytes) / BLOCK_SIZE + ((__bytes) % BLOCK_SIZE ? 1 : 0))

#define MALLOC_BLOCKS(__blks)                   (malloc(BLOCKS2BYTES(__blks)))

#define ALIGN_TO(__n, __align) ({                           \
    typeof (__n) __ret;                                     \
    if ((__n) % (__align)) {                                \
        __ret = ((__n) & (~((__align) - 1))) + (__align);   \
    } else __ret = (__n);                                   \
    __ret;                                                  \
})


#endif
