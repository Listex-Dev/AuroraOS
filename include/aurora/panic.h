#ifndef _AURORA_PANIC_H
#define _AURORA_PANIC_H

#include <aurora/types.h>

void panic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

#define kassert(expr) \
    do { \
        if (!(expr)) { \
            panic("Assertion failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        } \
    } while (0)

#endif /* _AURORA_PANIC_H */
