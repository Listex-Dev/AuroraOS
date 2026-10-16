#ifndef _AURORA_FS_FCNTL_H
#define _AURORA_FS_FCNTL_H

#include <aurora/types.h>

/* Open Flags */
#define O_RDONLY    00000000
#define O_WRONLY    00000001
#define O_RDWR      00000002
#define O_ACCMODE   00000003

#define O_CREAT     00000100 /* 0x40 */
#define O_EXCL      00000200 /* 0x80 */
#define O_NOCTTY    00000400 /* 0x100 */
#define O_TRUNC     00001000 /* 0x200 */
#define O_APPEND    00002000 /* 0x400 */
#define O_NONBLOCK  00004000 /* 0x800 */
#define O_DIRECTORY 00200000 /* 0x10000 */
#define O_CLOEXEC   02000000 /* 0x80000 */

/* File descriptor flags for fcntl */
#define FD_CLOEXEC  1

/* Whence flags for lseek */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Fcntl commands */
#define F_DUPFD     0
#define F_GETFD     1
#define F_SETFD     2
#define F_GETFL     3
#define F_SETFL     4

#endif /* _AURORA_FS_FCNTL_H */
