#ifndef _AURORA_DRIVERS_TTY_H
#define _AURORA_DRIVERS_TTY_H

#include <aurora/types.h>
#include <aurora/termios.h>
#include <aurora/fs/vfs.h>

#define TTY_BUF_SIZE 1024

#define NUM_VIRTUAL_TTYS 6
#define TTY_SERIAL_INDEX 64

struct tty {
    int index; /* 0 = active VT, 1..6 = VT1..VT6, 64 = ttyS0 */
    struct termios termios;
    struct winsize winsize;

    pid_t pgrp;
    pid_t session;

    /* Input read buffer (committed characters) */
    char in_buf[TTY_BUF_SIZE];
    int in_head;
    int in_tail;
    int in_count;

    /* Canonical line buffer (uncommitted editing buffer) */
    char line_buf[TTY_BUF_SIZE];
    int line_len;

    /* Device output method */
    void (*write_out)(struct tty *tty, const char *buf, size_t count);
};

void tty_subsys_init(void);
struct tty *tty_get(int index);
struct tty *tty_get_active_vt(void);

void tty_input_char(struct tty *tty, char c);
void tty_input_string(struct tty *tty, const char *str);

/* File operations for TTY devices */
extern struct file_operations g_tty_fops;

#endif /* _AURORA_DRIVERS_TTY_H */
