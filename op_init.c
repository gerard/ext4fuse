#include <fuse.h>
#include "logging.h"

void *op_init(struct fuse_conn_info *info)
{
    INFO("Using FUSE protocol %d.%d", info->proto_major, info->proto_minor);
    return NULL;
}
