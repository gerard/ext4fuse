#ifndef OPS_H
#define OPS_H

int e4f_readlink(const char *path, char *buf, size_t bufsize);
void *op_init(struct fuse_conn_info *info);

#endif
