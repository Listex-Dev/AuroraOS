#include <aurora/drivers/tty.h>
#include <aurora/drivers/console.h>
#include <aurora/fs/devtmpfs.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/serial.h>
#include <aurora/signal.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

extern int64_t sys_kill(pid_t pid, int sig);

static struct tty g_ttys[NUM_VIRTUAL_TTYS + 1]; /* 0: active VT, 1..6: VT1..VT6 */
static struct tty g_tty_serial;                 /* index 64: ttyS0 */

static void vt_tty_write_out(struct tty *tty, const char *buf, size_t count)
{
    int vt_num = (tty->index == 0) ? vt_get_active() : (tty->index - 1);
    vt_write(vt_num, buf, count);
}

static void serial_tty_write_out(struct tty *tty, const char *buf, size_t count)
{
    (void)tty;
    serial_write(buf, count);
}

static void tty_echo(struct tty *tty, const char *buf, size_t count)
{
    if (tty && tty->write_out) {
        tty->write_out(tty, buf, count);
    }
}

static void tty_init_defaults(struct tty *tty, int index, void (*write_out)(struct tty *, const char *, size_t))
{
    memset(tty, 0, sizeof(struct tty));
    tty->index = index;
    tty->write_out = write_out;

    /* termios defaults */
    tty->termios.c_iflag = ICRNL | IXON;
    tty->termios.c_oflag = OPOST | ONLCR;
    tty->termios.c_cflag = CS8 | CREAD | HUPCL | CLOCAL;
    tty->termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN;

    tty->termios.c_cc[VINTR] = 0x03; /* ^C */
    tty->termios.c_cc[VQUIT] = 0x1C; /* ^\ */
    tty->termios.c_cc[VERASE] = 0x7F; /* Backspace */
    tty->termios.c_cc[VKILL] = 0x15; /* ^U */
    tty->termios.c_cc[VEOF] = 0x04;  /* ^D */
    tty->termios.c_cc[VTIME] = 0;
    tty->termios.c_cc[VMIN] = 1;
    tty->termios.c_cc[VSUSP] = 0x1A; /* ^Z */
    tty->termios.c_cc[VSTART] = 0x11;/* ^Q */
    tty->termios.c_cc[VSTOP] = 0x13; /* ^S */

    /* default window size: 80x25 */
    tty->winsize.ws_row = VGA_HEIGHT;
    tty->winsize.ws_col = VGA_WIDTH;
    tty->winsize.ws_xpixel = 0;
    tty->winsize.ws_ypixel = 0;

    tty->pgrp = 0;
    tty->session = 0;
}

struct tty *tty_get_active_vt(void)
{
    int active = vt_get_active();
    return &g_ttys[active + 1];
}

struct tty *tty_get(int index)
{
    if (index == 0) {
        return tty_get_active_vt();
    }
    if (index >= 1 && index <= NUM_VIRTUAL_TTYS) {
        return &g_ttys[index];
    }
    if (index == TTY_SERIAL_INDEX) {
        return &g_tty_serial;
    }
    return NULL;
}

static void tty_queue_put(struct tty *tty, char c)
{
    if (tty->in_count < TTY_BUF_SIZE) {
        tty->in_buf[tty->in_tail] = c;
        tty->in_tail = (tty->in_tail + 1) % TTY_BUF_SIZE;
        tty->in_count++;
    }
}

static int tty_queue_get(struct tty *tty, char *c)
{
    if (tty->in_count == 0) {
        return 0;
    }
    *c = tty->in_buf[tty->in_head];
    tty->in_head = (tty->in_head + 1) % TTY_BUF_SIZE;
    tty->in_count--;
    return 1;
}

void tty_input_char(struct tty *tty, char c)
{
    if (!tty) return;

    /* 1. Signal handling if ISIG enabled */
    if (tty->termios.c_lflag & ISIG) {
        if (c == (char)tty->termios.c_cc[VINTR]) {
            /* Ctrl-C */
            if (tty->termios.c_lflag & ECHO) {
                tty_echo(tty, "^C\n", 3);
            }
            if (!(tty->termios.c_lflag & NOFLSH)) {
                tty->line_len = 0;
            }
            if (tty->pgrp > 0) {
                sys_kill(-tty->pgrp, SIGINT);
            }
            return;
        }

        if (c == (char)tty->termios.c_cc[VQUIT]) {
            /* Ctrl-\ */
            if (tty->termios.c_lflag & ECHO) {
                tty_echo(tty, "^\\\n", 3);
            }
            if (tty->pgrp > 0) {
                sys_kill(-tty->pgrp, SIGQUIT);
            }
            return;
        }

        if (c == (char)tty->termios.c_cc[VSUSP]) {
            /* Ctrl-Z */
            if (tty->termios.c_lflag & ECHO) {
                tty_echo(tty, "^Z\n", 3);
            }
            if (tty->pgrp > 0) {
                sys_kill(-tty->pgrp, SIGTSTP);
            }
            return;
        }
    }

    /* 2. Canonical mode processing */
    if (tty->termios.c_lflag & ICANON) {
        if (c == '\r' && (tty->termios.c_iflag & ICRNL)) {
            c = '\n';
        }

        if (c == '\n') {
            if (tty->termios.c_lflag & (ECHO | ECHONL)) {
                tty_echo(tty, "\n", 1);
            }
            if (tty->line_len < TTY_BUF_SIZE - 1) {
                tty->line_buf[tty->line_len++] = '\n';
            }
            /* Commit line buffer into input queue */
            for (int i = 0; i < tty->line_len; i++) {
                tty_queue_put(tty, tty->line_buf[i]);
            }
            tty->line_len = 0;
            return;
        }

        if (c == (char)tty->termios.c_cc[VERASE] || c == '\b') {
            if (tty->line_len > 0) {
                tty->line_len--;
                if (tty->termios.c_lflag & ECHO) {
                    if (tty->termios.c_lflag & ECHOE) {
                        tty_echo(tty, "\b \b", 3);
                    } else {
                        tty_echo(tty, "\b", 1);
                    }
                }
            }
            return;
        }

        if (c == (char)tty->termios.c_cc[VKILL]) {
            /* Ctrl-U: erase entire line */
            if (tty->termios.c_lflag & ECHO) {
                for (int i = 0; i < tty->line_len; i++) {
                    tty_echo(tty, "\b \b", 3);
                }
            }
            tty->line_len = 0;
            return;
        }

        if (c == (char)tty->termios.c_cc[VEOF]) {
            /* Ctrl-D: commit line up to EOF without adding ^D */
            for (int i = 0; i < tty->line_len; i++) {
                tty_queue_put(tty, tty->line_buf[i]);
            }
            tty->line_len = 0;
            return;
        }

        /* Normal printable character or tab */
        if ((uint8_t)c >= 32 || c == '\t') {
            if (tty->line_len < TTY_BUF_SIZE - 2) {
                tty->line_buf[tty->line_len++] = c;
                if (tty->termios.c_lflag & ECHO) {
                    tty_echo(tty, &c, 1);
                }
            }
            return;
        }
    } else {
        /* 3. Raw / Non-canonical mode */
        tty_queue_put(tty, c);
        if (tty->termios.c_lflag & ECHO) {
            tty_echo(tty, &c, 1);
        }
    }
}

void tty_input_string(struct tty *tty, const char *str)
{
    if (!tty || !str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        tty_input_char(tty, str[i]);
    }
}

/* File operations implementation */
static struct tty *get_tty_from_file(struct file *file)
{
    if (!file) return NULL;
    if (file->private_data) {
        return (struct tty *)file->private_data;
    }

    /* Check inode minor if available */
    if (file->f_inode) {
        uint32_t major = (uint32_t)(file->f_inode->i_rdev >> 8);
        uint32_t minor = (uint32_t)(file->f_inode->i_rdev & 0xFF);

        if (major == 5 && minor == 0) {
            /* /dev/tty -> current process tty */
            task_t *curr = sched_current();
            if (curr && curr->tty) return (struct tty *)curr->tty;
            return tty_get_active_vt();
        }
        if (major == 5 && minor == 1) {
            /* /dev/console -> active VT */
            return tty_get_active_vt();
        }
        if (major == 4) {
            return tty_get((int)minor);
        }
    }

    return tty_get_active_vt();
}

static int tty_open(struct inode *inode, struct file *file)
{
    if (!inode || !file) return -EINVAL;

    uint32_t major = (uint32_t)(inode->i_rdev >> 8);
    uint32_t minor = (uint32_t)(inode->i_rdev & 0xFF);
    struct tty *target = NULL;

    if (major == 5 && minor == 0) {
        task_t *curr = sched_current();
        target = curr ? (struct tty *)curr->tty : NULL;
        if (!target) target = tty_get_active_vt();
    } else if (major == 5 && minor == 1) {
        target = tty_get_active_vt();
    } else if (major == 4) {
        target = tty_get((int)minor);
    }

    if (!target) return -ENODEV;
    file->private_data = target;

    /* Associate controlling tty if not yet set */
    task_t *curr = sched_current();
    if (curr && !curr->tty) {
        curr->tty = target;
        if (target->pgrp == 0) {
            target->pgrp = curr->pgid ? curr->pgid : curr->pid;
        }
    }

    return 0;
}

static int tty_release(struct inode *inode, struct file *file)
{
    (void)inode;
    if (file) {
        file->private_data = NULL;
    }
    return 0;
}

static ssize_t tty_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct tty *tty = get_tty_from_file(file);
    if (!tty || !buf) return -EFAULT;
    if (count == 0) return 0;

    char *out = (char *)buf;
    size_t read_bytes = 0;

    while (read_bytes < count) {
        char c;
        if (tty_queue_get(tty, &c)) {
            out[read_bytes++] = c;
            if ((tty->termios.c_lflag & ICANON) && c == '\n') {
                break;
            }
        } else {
            /* If we already read some bytes, return them immediately */
            if (read_bytes > 0) {
                break;
            }
            /* Nothing available */
            if (file->f_flags & O_NONBLOCK) {
                return -EAGAIN;
            }
            break; /* Non-blocking return for now in kernel environment */
        }
    }

    return (ssize_t)read_bytes;
}

static ssize_t tty_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct tty *tty = get_tty_from_file(file);
    if (!tty || !buf) return -EFAULT;
    if (count == 0) return 0;

    const char *src = (const char *)buf;

    if (tty->termios.c_oflag & OPOST) {
        for (size_t i = 0; i < count; i++) {
            if (src[i] == '\n' && (tty->termios.c_oflag & ONLCR)) {
                if (tty->write_out) tty->write_out(tty, "\r\n", 2);
            } else {
                if (tty->write_out) tty->write_out(tty, &src[i], 1);
            }
        }
    } else {
        if (tty->write_out) {
            tty->write_out(tty, src, count);
        }
    }

    return (ssize_t)count;
}

static int tty_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct tty *tty = get_tty_from_file(file);
    if (!tty) return -ENODEV;

    switch (cmd) {
        case TCGETS:
        case TCGETA:
            if (!arg) return -EFAULT;
            memcpy((void *)arg, &tty->termios, sizeof(struct termios));
            return 0;

        case TCSETS:
        case TCSETSW:
        case TCSETSF:
        case TCSETA:
            if (!arg) return -EFAULT;
            memcpy(&tty->termios, (const void *)arg, sizeof(struct termios));
            return 0;

        case TIOCGWINSZ:
            if (!arg) return -EFAULT;
            memcpy((void *)arg, &tty->winsize, sizeof(struct winsize));
            return 0;

        case TIOCSWINSZ:
            if (!arg) return -EFAULT;
            memcpy(&tty->winsize, (const void *)arg, sizeof(struct winsize));
            return 0;

        case TIOCGPGRP:
            if (!arg) return -EFAULT;
            *(pid_t *)arg = tty->pgrp;
            return 0;

        case TIOCSPGRP: {
            if (!arg) return -EFAULT;
            tty->pgrp = *(const pid_t *)arg;
            task_t *c = sched_current();
            if (c) {
                if (c->pid == tty->pgrp) {
                    c->pgid = tty->pgrp;
                }
            }
            return 0;
        }

        case TIOCSCTTY: {
            task_t *curr = sched_current();
            if (curr) {
                curr->tty = tty;
                tty->session = curr->sid ? curr->sid : curr->pid;
                tty->pgrp = curr->pgid ? curr->pgid : curr->pid;
            }
            return 0;
        }

        case TIOCNOTTY: {
            task_t *curr = sched_current();
            if (curr && curr->tty == tty) {
                curr->tty = NULL;
            }
            return 0;
        }

        case FIONREAD:
            if (!arg) return -EFAULT;
            *(int *)arg = tty->in_count;
            return 0;

        case VT_ACTIVATE: {
            int vt = (int)arg;
            if (vt >= 1 && vt <= NUM_VIRTUAL_TTYS) {
                vt_switch(vt - 1);
                return 0;
            }
            return -EINVAL;
        }

        case VT_GETSTATE:
            if (!arg) return -EFAULT;
            *(int *)arg = vt_get_active() + 1;
            return 0;

        default:
            return -EINVAL;
    }
}

struct file_operations g_tty_fops = {
    .open = tty_open,
    .release = tty_release,
    .read = tty_read,
    .write = tty_write,
    .ioctl = tty_ioctl,
};

void tty_subsys_init(void)
{
    /* Initialize console hardware first */
    console_init();

    /* 1. Initialize VT ttys: 0 = active alias, 1..6 = VT1..VT6 */
    for (int i = 0; i <= NUM_VIRTUAL_TTYS; i++) {
        tty_init_defaults(&g_ttys[i], i, vt_tty_write_out);
    }

    /* 2. Initialize serial tty (ttyS0) */
    tty_init_defaults(&g_tty_serial, TTY_SERIAL_INDEX, serial_tty_write_out);

    /* 3. Register standard character devices in devtmpfs */
    devtmpfs_register_chrdev(4, 0, "tty0", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 1, "tty1", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 2, "tty2", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 3, "tty3", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 4, "tty4", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 5, "tty5", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 6, "tty6", &g_tty_fops, 0620);
    devtmpfs_register_chrdev(4, 64, "ttyS0", &g_tty_fops, 0660);
    devtmpfs_register_chrdev(5, 1, "console", &g_tty_fops, 0600);
    devtmpfs_register_chrdev(5, 0, "tty", &g_tty_fops, 0666);

    printk("<6>[  0.000000] TTY: Subsystem initialized (VT1-VT6, tty0, ttyS0, console, tty)\n");
}
