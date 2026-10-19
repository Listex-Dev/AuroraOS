#include <aurora/drivers/pty.h>
#include <aurora/drivers/tty.h>
#include <aurora/fs/devtmpfs.h>
#include <aurora/proc/sched.h>
#include <aurora/signal.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

extern int64_t sys_kill(pid_t pid, int sig);

static struct pty_pair s_ptys[MAX_PTY_PAIRS];

static void pty_slave_write_out(struct tty *tty, const char *buf, size_t count)
{
    if (!tty || !buf || count == 0) return;
    struct pty_pair *pty = (struct pty_pair *)((uint8_t *)tty - offsetof(struct pty_pair, slave_tty));
    if (!pty || !pty->allocated) return;

    for (size_t i = 0; i < count; i++) {
        if (pty->master_count < PTY_BUFFER_SIZE) {
            pty->master_buf[pty->master_head] = buf[i];
            pty->master_head = (pty->master_head + 1) % PTY_BUFFER_SIZE;
            pty->master_count++;
        }
    }
}

/* Master file operations (/dev/ptmx) */
static int ptmx_open(struct inode *inode, struct file *file)
{
    (void)inode;
    if (!file) return -EINVAL;

    for (int i = 0; i < MAX_PTY_PAIRS; i++) {
        if (!s_ptys[i].allocated) {
            struct pty_pair *pty = &s_ptys[i];
            memset(pty, 0, sizeof(struct pty_pair));
            pty->index = i;
            pty->allocated = true;
            pty->locked = true;
            pty->master_open = true;
            pty->slave_open = false;

            /* Initialize termios line discipline defaults for slave_tty */
            pty->slave_tty.index = i;
            pty->slave_tty.write_out = pty_slave_write_out;

            pty->slave_tty.termios.c_iflag = ICRNL | IXON;
            pty->slave_tty.termios.c_oflag = OPOST | ONLCR;
            pty->slave_tty.termios.c_cflag = CS8 | CREAD | HUPCL | CLOCAL;
            pty->slave_tty.termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN;

            pty->slave_tty.termios.c_cc[VINTR]  = 0x03; /* ^C */
            pty->slave_tty.termios.c_cc[VQUIT]  = 0x1C; /* ^\ */
            pty->slave_tty.termios.c_cc[VERASE] = 0x7F; /* Backspace */
            pty->slave_tty.termios.c_cc[VKILL]  = 0x15; /* ^U */
            pty->slave_tty.termios.c_cc[VEOF]   = 0x04; /* ^D */
            pty->slave_tty.termios.c_cc[VTIME]  = 0;
            pty->slave_tty.termios.c_cc[VMIN]   = 1;
            pty->slave_tty.termios.c_cc[VSUSP]  = 0x1A; /* ^Z */
            pty->slave_tty.termios.c_cc[VSTART] = 0x11;
            pty->slave_tty.termios.c_cc[VSTOP]  = 0x13;

            pty->slave_tty.winsize.ws_row = 24;
            pty->slave_tty.winsize.ws_col = 80;

            file->private_data = pty;
            return 0;
        }
    }
    return -EMFILE;
}

static int ptmx_release(struct inode *inode, struct file *file)
{
    (void)inode;
    if (!file) return -EINVAL;
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (pty) {
        pty->master_open = false;
        if (!pty->slave_open) {
            pty->allocated = false;
        }
        file->private_data = NULL;
    }
    return 0;
}

static ssize_t ptmx_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (!pty || !pty->allocated || !buf) return -EFAULT;
    if (count == 0) return 0;

    char *out = (char *)buf;
    size_t read_bytes = 0;

    while (read_bytes < count) {
        if (pty->master_count > 0) {
            out[read_bytes++] = pty->master_buf[pty->master_tail];
            pty->master_tail = (pty->master_tail + 1) % PTY_BUFFER_SIZE;
            pty->master_count--;
        } else {
            if (read_bytes > 0) break;
            if (!pty->slave_open) {
                return 0; /* EOF */
            }
            if (file->f_flags & O_NONBLOCK) {
                return -EAGAIN;
            }
            sched_yield();
        }
    }
    return (ssize_t)read_bytes;
}

static ssize_t ptmx_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (!pty || !pty->allocated || !buf) return -EFAULT;
    if (count == 0) return 0;

    const char *src = (const char *)buf;
    for (size_t i = 0; i < count; i++) {
        tty_input_char(&pty->slave_tty, src[i]);
    }
    return (ssize_t)count;
}

static int ptmx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (!pty || !pty->allocated) return -ENODEV;

    switch (cmd) {
        case TIOCGPTN:
            if (!arg) return -EFAULT;
            *(unsigned int *)arg = (unsigned int)pty->index;
            return 0;

        case TIOCSPTLCK:
            if (!arg) return -EFAULT;
            pty->locked = (*(const int *)arg != 0);
            return 0;

        case TIOCGWINSZ:
            if (!arg) return -EFAULT;
            memcpy((void *)arg, &pty->slave_tty.winsize, sizeof(struct winsize));
            return 0;

        case TIOCSWINSZ:
            if (!arg) return -EFAULT;
            memcpy(&pty->slave_tty.winsize, (const void *)arg, sizeof(struct winsize));
            if (pty->slave_tty.pgrp > 0) {
                sys_kill(-pty->slave_tty.pgrp, SIGWINCH);
            }
            return 0;

        case FIONREAD:
            if (!arg) return -EFAULT;
            *(int *)arg = pty->master_count;
            return 0;

        default:
            return -EINVAL;
    }
}

struct file_operations g_ptmx_fops = {
    .open    = ptmx_open,
    .release = ptmx_release,
    .read    = ptmx_read,
    .write   = ptmx_write,
    .ioctl   = ptmx_ioctl,
};

/* Slave file operations (/dev/pts/N) */
static int pty_slave_open(struct inode *inode, struct file *file)
{
    if (!inode || !file) return -EINVAL;
    uint32_t minor = (uint32_t)(inode->i_rdev & 0xFF);
    if (minor >= MAX_PTY_PAIRS) return -ENXIO;

    struct pty_pair *pty = &s_ptys[minor];
    if (!pty->allocated || pty->locked) {
        return -EIO;
    }

    pty->slave_open = true;
    file->private_data = pty;

    task_t *curr = sched_current();
    if (curr && !curr->tty) {
        curr->tty = &pty->slave_tty;
        if (pty->slave_tty.pgrp == 0) {
            pty->slave_tty.pgrp = curr->pgid ? curr->pgid : curr->pid;
        }
    }
    return 0;
}

static int pty_slave_release(struct inode *inode, struct file *file)
{
    (void)inode;
    if (!file) return -EINVAL;
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (pty) {
        pty->slave_open = false;
        if (!pty->master_open) {
            pty->allocated = false;
        }
        file->private_data = NULL;
    }
    return 0;
}

static ssize_t pty_slave_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (!pty || !pty->allocated || !buf) return -EFAULT;
    if (count == 0) return 0;

    struct tty *tty = &pty->slave_tty;
    char *out = (char *)buf;
    size_t read_bytes = 0;

    while (read_bytes < count) {
        if (tty->in_count > 0) {
            char c = tty->in_buf[tty->in_head];
            tty->in_head = (tty->in_head + 1) % TTY_BUF_SIZE;
            tty->in_count--;
            out[read_bytes++] = c;
            if ((tty->termios.c_lflag & ICANON) && c == '\n') {
                break;
            }
        } else {
            if (read_bytes > 0) break;
            if (!pty->master_open) {
                return 0; /* EOF */
            }
            if (file->f_flags & O_NONBLOCK) {
                return -EAGAIN;
            }
            sched_yield();
        }
    }
    return (ssize_t)read_bytes;
}

static ssize_t pty_slave_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (!pty || !pty->allocated || !buf) return -EFAULT;
    if (!pty->master_open) return -EIO;
    if (count == 0) return 0;

    const char *src = (const char *)buf;
    struct tty *tty = &pty->slave_tty;

    if (tty->termios.c_oflag & OPOST) {
        for (size_t i = 0; i < count; i++) {
            if (src[i] == '\n' && (tty->termios.c_oflag & ONLCR)) {
                pty_slave_write_out(tty, "\r\n", 2);
            } else {
                pty_slave_write_out(tty, &src[i], 1);
            }
        }
    } else {
        pty_slave_write_out(tty, src, count);
    }

    return (ssize_t)count;
}

static int pty_slave_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct pty_pair *pty = (struct pty_pair *)file->private_data;
    if (!pty || !pty->allocated) return -ENODEV;

    struct tty *tty = &pty->slave_tty;

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
            if (tty->pgrp > 0) {
                sys_kill(-tty->pgrp, SIGWINCH);
            }
            return 0;

        case TIOCGPGRP:
            if (!arg) return -EFAULT;
            *(pid_t *)arg = tty->pgrp;
            return 0;

        case TIOCSPGRP: {
            if (!arg) return -EFAULT;
            tty->pgrp = *(const pid_t *)arg;
            task_t *c = sched_current();
            if (c && c->pid == tty->pgrp) {
                c->pgid = tty->pgrp;
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

        default:
            return -EINVAL;
    }
}

struct file_operations g_pty_slave_fops = {
    .open    = pty_slave_open,
    .release = pty_slave_release,
    .read    = pty_slave_read,
    .write   = pty_slave_write,
    .ioctl   = pty_slave_ioctl,
};

struct pty_pair *pty_get_pair(int index)
{
    if (index >= 0 && index < MAX_PTY_PAIRS) {
        return &s_ptys[index];
    }
    return NULL;
}

void pty_subsys_init(void)
{
    memset(s_ptys, 0, sizeof(s_ptys));

    /* 1. Register /dev/ptmx */
    devtmpfs_register_chrdev(PTMX_MAJOR, PTMX_MINOR, "ptmx", &g_ptmx_fops, 0666);

    /* 2. Create /dev/pts directory and nodes 0..7 */
    struct dentry *pts_dir = devtmpfs_mkdir("pts", 0755);
    if (pts_dir) {
        char name[8];
        for (int i = 0; i < 8; i++) {
            name[0] = '0' + i;
            name[1] = '\0';
            devtmpfs_register_chrdev_at(pts_dir, PTS_MAJOR, i, name, &g_pty_slave_fops, 0620);
        }
    }

    printk("<6>[  0.000000] PTY: Unix98 pseudo-terminal subsystem initialized (/dev/ptmx, /dev/pts/0..7)\n");
}
