#ifndef LOGGING_H
#define LOGGING_H

#include <assert.h>

#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#define EMERG(...)              __LOG(LOG_EMERG, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define ALERT(...)              __LOG(LOG_ALERT, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define CRIT(...)               __LOG(LOG_CRIT, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define ERR(...)                __LOG(LOG_ERR, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define WARNING(...)            __LOG(LOG_WARNING, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define NOTICE(...)             __LOG(LOG_NOTICE, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define INFO(...)               __LOG(LOG_INFO, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define DEBUG(...)              __LOG(LOG_DEBUG, __FUNCTION__, __LINE__, ##__VA_ARGS__);

#define DEFAULT_LOG_FILE        NULL

#ifndef NDEBUG
#define DEFAULT_LOG_LEVEL LOG_DEBUG
#define ASSERT(assertion) ({                            \
    if (!(assertion)) {                                 \
        WARNING("ASSERT FAIL: " #assertion);            \
        assert(assertion);                              \
    }                                                   \
})
#else
#define DEFAULT_LOG_LEVEL LOG_ERR
#define ASSERT(assertion)       do { } while(0)
#endif

void __LOG(int level, const char *func, int line, const char *format, ...);
void logging_setlevel(int new_level);
int logging_open(const char *path);

#endif
