#ifndef OPS_H
#define OPS_H

struct fuse_conn_info;
struct fuse_file_info;

int e4f_readlink(const char *path, char *buf, size_t bufsize);
void *op_init(struct fuse_conn_info *info);
int e4f_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);

#endif
