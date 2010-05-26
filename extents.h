#ifndef EXTENTS_H
#define EXTENTS_H

#include "types/ext4_extents.h"

struct ext4_inode_extent {
    struct ext4_extent_header eh;
    union {
        struct ext4_extent ee[4];
        struct ext4_extent_idx ei[4];
    };
};

uint64_t extent_get_pblock(struct ext4_inode_extent *inode_ext, uint32_t lblock, uint32_t *extent);

#endif
