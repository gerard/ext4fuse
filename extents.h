#ifndef EXTENTS_H
#define EXTENTS_H

#include "types/ext4_extents.h"

uint64_t extent_get_pblock(void *inode_extents, uint32_t lblock, uint32_t *len);

#endif
