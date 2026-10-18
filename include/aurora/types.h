#ifndef _AURORA_TYPES_H
#define _AURORA_TYPES_H

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef int64_t            intptr_t;
typedef uint64_t           uintptr_t;

typedef uint64_t           size_t;
typedef int64_t            ssize_t;
typedef int64_t            ptrdiff_t;

typedef int32_t            pid_t;
typedef uint32_t           uid_t;
typedef uint32_t           gid_t;
typedef uint64_t           ino_t;
typedef uint32_t           mode_t;
typedef int64_t            off_t;
typedef uint32_t           dev_t;
typedef int64_t            time_t;

struct timespec {
    time_t tv_sec;
    int64_t tv_nsec;
};

#ifndef bool
typedef _Bool              bool;
#define true               1
#define false              0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* _AURORA_TYPES_H */
