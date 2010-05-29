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
#define MIN(x, y)   ({                  \
    typeof (x) __x = (x);               \
    typeof (y) __y = (y);               \
    __x < __y ? __x : __y;              \
})


#endif
