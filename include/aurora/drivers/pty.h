#ifndef _AURORA_DRIVERS_PTY_H
#define _AURORA_DRIVERS_PTY_H

#include <aurora/types.h>
#include <aurora/termios.h>
#include <aurora/drivers/tty.h>
#include <aurora/fs/vfs.h>

#define MAX_PTY_PAIRS   16
#define PTY_BUFFER_SIZE 2048

#define PTMX_MAJOR      5
#define PTMX_MINOR      2

#define PTS_MAJOR       136

/* PTY Pair representation */
struct pty_pair {
    int index;                  /* 0 .. MAX_PTY_PAIRS-1 */
    bool allocated;             /* Pair is active */
    bool locked;                /* Slave node locked (TIOCSPTLCK) */
    bool master_open;           /* Master file descriptor open */
    bool slave_open;            /* Slave file descriptor open */

    struct tty slave_tty;       /* Termios line discipline state for slave */

    /* Master read buffer: receives slave output & echoes */
    char master_buf[PTY_BUFFER_SIZE];
    int master_head;
    int master_tail;
    int master_count;
};

void pty_subsys_init(void);
struct pty_pair *pty_get_pair(int index);

extern struct file_operations g_ptmx_fops;
extern struct file_operations g_pty_slave_fops;

#endif /* _AURORA_DRIVERS_PTY_H */
