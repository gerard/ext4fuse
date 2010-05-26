#ifndef COMMON_H
#define COMMON_H

#define ALIGN_TO(__n, __align) ({                           \
    typeof (__n) __ret;                                     \
    if ((__n) % (__align)) {                                \
        __ret = ((__n) & (~((__align) - 1))) + (__align);   \
    } else __ret = (__n);                                   \
    __ret;                                                  \
})

#define UNUSED(__x)     ((void)(__x))

#endif
