#ifndef OPS_H
#define OPS_H

#include <fuse.h>

void *op_init(struct fuse_conn_info *info);
int op_readlink(const char *path, char *buf, size_t bufsize);
int op_read(const char *path, char *buf, size_t size, off_t offset
                             , struct fuse_file_info *fi);
int op_readdir(const char *path, void *buf, fuse_fill_dir_t filler
                               , off_t offset, struct fuse_file_info *fi);
int op_getattr(const char *path, struct stat *stbuf);
int op_open(const char *path, struct fuse_file_info *fi);

#endif
