#ifndef DCACHE_H
#define DCACHE_H

#include <stdint.h>

struct dcache_entry;
struct dcache_entry *dcache_insert(struct dcache_entry *parent, const char *name, int namelen, uint32_t n);
struct dcache_entry *dcache_lookup(struct dcache_entry *parent, const char *name, int namelen);
uint32_t dcache_get_inode(struct dcache_entry *entry);
int dcache_init_root(uint32_t n);

#endif
