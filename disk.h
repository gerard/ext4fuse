#define read_disk(__where, __size, __p)         __read_disk(__where, __size, __p, __func__, __LINE__)
#define read_disk_block(__block, __p)           read_disk_blocks(__block, 1, __p)
#define read_disk_blocks(__blocks, __n, __p)    __read_disk(BLOCKS2BYTES(__blocks), BLOCKS2BYTES(__n), __p, __func__, __LINE__)

int __read_disk(off_t where, size_t size, void *p, const char *func, int line);
int open_disk(const char *path);
