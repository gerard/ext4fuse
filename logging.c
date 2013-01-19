/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */


#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include "logging.h"

static int loglevel = DEFAULT_LOG_LEVEL;
static FILE *logfile = NULL;
static const char *loglevel_str[] = {
    [LOG_EMERG]     = "[emerg]",
    [LOG_ALERT]     = "[alert]",
    [LOG_CRIT]      = "[crit] ",
    [LOG_ERR]       = "[err]  ",
    [LOG_WARNING]   = "[warn] ",
    [LOG_NOTICE]    = "[notic]",
    [LOG_INFO]      = "[info] ",
    [LOG_DEBUG]     = "[debug]",
};

void __LOG(int level, const char *func, int line, const char *format, ...)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    va_list ap;

    if (!logfile) return;
    if (level < 0) return;
    if (level > loglevel) return;

    /* We have a lock here so different threads don interleave the log output */
    pthread_mutex_lock(&lock);
    va_start(ap, format);
    fprintf(logfile, "%s[%s:%d] ", loglevel_str[level], func, line);
    vfprintf(logfile, format, ap);
    fprintf(logfile, "\n");
    va_end(ap);
    pthread_mutex_unlock(&lock);
}

void logging_setlevel(int new_level)
{
    loglevel = new_level;
}


int logging_open(const char *path)
{
    if (path == NULL) return 0;

    if ((logfile = fopen(path, "w")) == NULL) {
        perror("open");
        return -1;
    }

    return 0;
}
