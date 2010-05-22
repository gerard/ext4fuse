#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include "e4flib.h"

static int disk_fd = -1;

int __read_disk(off_t where, size_t size, void *p, const char *func, int line)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    ssize_t read_ret;
    off_t lseek_ret;

    E4F_ASSERT(disk_fd >= 0);

    pthread_mutex_lock(&lock);
    E4F_DEBUG("Disk Read: 0x%jd +0x%zx [%s:%d]", where, size, func, line);
    lseek_ret = lseek(disk_fd, where, SEEK_SET);
    read_ret = read(disk_fd, p, size);
    pthread_mutex_unlock(&lock);

    E4F_ASSERT(lseek_ret == where);
    E4F_ASSERT(read_ret == size);

    return 0;
}

int open_disk(const char *path)
{
    disk_fd = open(path, O_RDONLY);
    if (disk_fd < 0) {
        perror("open");
        return -1;
    }

    return 0;
}
