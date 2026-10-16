#ifndef _AURORA_SYSCALL_H
#define _AURORA_SYSCALL_H

#include <aurora/types.h>

/* Linux x86-64 syscall numbers */
#define SYS_read     0
#define SYS_write    1
#define SYS_open     2
#define SYS_close    3
#define SYS_stat     4
#define SYS_fstat    5
#define SYS_brk      12
#define SYS_getpid   39
#define SYS_fork     57
#define SYS_execve   59
#define SYS_exit     60
#define SYS_wait4    61

void syscall_init(void);

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

/* Syscall implementations */
int64_t sys_write(int fd, const void *buf, size_t count);
int64_t sys_exit(int status);
int64_t sys_getpid(void);
int64_t sys_brk(uintptr_t new_brk);

#endif /* _AURORA_SYSCALL_H */
