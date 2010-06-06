#ifndef E4F_DCACHE_H
#define E4F_DCACHE_H

#include <stdint.h>

#include "../common.h"

#ifdef __64BITS
#define DCACHE_ENTRY_NAME_LEN       40
#else
#define DCACHE_ENTRY_NAME_LEN       48
#endif


/* This struct declares a node of a k-tree.  Every node has a pointer to one of
 * the childs and a pointer (in a circular list fashion) to its siblings. */

struct dcache_entry {
    struct dcache_entry *childs;
    struct dcache_entry *siblings;
    uint32_t inode;
    uint16_t lru_count;
    uint8_t user_count;
    char name[DCACHE_ENTRY_NAME_LEN];
};

/* Keep the struct cacheline friendly */
STATIC_ASSERT(sizeof(struct dcache_entry) == 64);

#endif
