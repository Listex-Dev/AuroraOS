#include <aurora/fs/procfs.h>
#include <aurora/fs/mount.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/heap.h>
#include <aurora/bootinfo.h>
#include <aurora/arch/x86_64/pit.h>
#include <aurora/proc/sched.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>
#include <aurora/drivers/block.h>

struct proc_node {
    proc_read_fn generator;
};

static ssize_t proc_file_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    struct inode *inode = file->f_inode;
    if (!inode || !inode->i_private) {
        return 0;
    }

    struct proc_node *node = (struct proc_node *)inode->i_private;
    if (!node->generator) {
        return 0;
    }

    char kbuf[1024];
    ssize_t total = node->generator(kbuf, sizeof(kbuf));
    if (total <= 0) {
        return 0;
    }

    if (*pos >= (uint64_t)total) {
        return 0; /* EOF */
    }

    size_t to_read = count;
    if (*pos + to_read > (size_t)total) {
        to_read = total - *pos;
    }

    memcpy(buf, kbuf + *pos, to_read);
    *pos += to_read;
    return (ssize_t)to_read;
}

static uint64_t proc_file_llseek(struct file *file, uint64_t offset, int whence)
{
    if (whence == SEEK_SET) {
        file->f_pos = offset;
        return file->f_pos;
    }
    return (uint64_t)-EINVAL;
}

static struct file_operations proc_file_ops = {
    .read = proc_file_read,
    .llseek = proc_file_llseek,
};

/* --- Generator Functions --- */

static ssize_t gen_meminfo(char *buf, size_t max_len)
{
    struct pmm_stats stats;
    pmm_get_stats(&stats);

    uint64_t total_kb = (stats.total_pages * PAGE_SIZE) / 1024;
    uint64_t free_kb = (stats.free_pages * PAGE_SIZE) / 1024;
    uint64_t avail_kb = free_kb;

    return snprintf(buf, max_len,
                    "MemTotal:       %8llu kB\n"
                    "MemFree:        %8llu kB\n"
                    "MemAvailable:   %8llu kB\n"
                    "Buffers:               0 kB\n"
                    "Cached:                0 kB\n",
                    total_kb, free_kb, avail_kb);
}

static ssize_t gen_cpuinfo(char *buf, size_t max_len)
{
    return snprintf(buf, max_len,
                    "processor       : 0\n"
                    "vendor_id       : AuthenticAMD\n"
                    "cpu family      : 15\n"
                    "model           : 1\n"
                    "model name      : x86-64 Compatible Processor\n"
                    "cpu MHz         : 2000.000\n"
                    "flags           : fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx lm\n");
}

static ssize_t gen_version(char *buf, size_t max_len)
{
    return snprintf(buf, max_len,
                    "AuroraOS version 1.0.0 (gcc) #1 SMP x86_64 AuroraOS libc\n");
}

static ssize_t gen_uptime(char *buf, size_t max_len)
{
    uint64_t ticks = pit_get_ticks();
    uint64_t sec = ticks / 100;
    uint64_t centisec = ticks % 100;
    return snprintf(buf, max_len, "%llu.%02llu %llu.%02llu\n",
                    sec, centisec, sec, centisec);
}

static ssize_t gen_loadavg(char *buf, size_t max_len)
{
    return snprintf(buf, max_len, "0.00 0.00 0.00 1/1 1\n");
}

static ssize_t gen_cmdline(char *buf, size_t max_len)
{
    const char *cmd = g_boot_info.cmdline ? g_boot_info.cmdline : "";
    return snprintf(buf, max_len, "%s\n", cmd);
}

static ssize_t gen_filesystems(char *buf, size_t max_len)
{
    return snprintf(buf, max_len,
                    "nodev\tramfs\n"
                    "nodev\tprocfs\n"
                    "nodev\tdevtmpfs\n"
                    "nodev\ttmpfs\n"
                    "nodev\tsysfs\n");
}

static ssize_t gen_devices(char *buf, size_t max_len)
{
    return snprintf(buf, max_len,
                    "Character devices:\n"
                    "  1 mem\n"
                    "  4 tty\n"
                    "  5 tty\n\n"
                    "Block devices:\n"
                    "  1 ramdisk\n"
                    "  8 sd\n");
}

static ssize_t gen_partitions(char *buf, size_t max_len)
{
    size_t off = 0;
    int len = snprintf(buf + off, max_len - off, "major minor  #blocks  name\n\n");
    if (len > 0) off += (size_t)len;

    for (struct block_device *b = bdev_get_list(); b != NULL; b = b->next) {
        uint32_t major = (uint32_t)(b->dev_id >> 8);
        uint32_t minor = (uint32_t)(b->dev_id & 0xFF);
        uint64_t blocks_1k = (b->total_blocks * b->block_size) / 1024;
        len = snprintf(buf + off, max_len - off, "%4u  %4u %10llu %s\n",
                       major, minor, blocks_1k, b->name);
        if (len > 0 && off + (size_t)len < max_len) {
            off += (size_t)len;
        } else {
            break;
        }
    }

    return (ssize_t)off;
}

static ssize_t gen_mounts(char *buf, size_t max_len)
{
    return vfs_format_mounts(buf, max_len);
}

static ssize_t gen_self_status(char *buf, size_t max_len)
{
    task_t *curr = sched_current();
    if (!curr) {
        return snprintf(buf, max_len, "Name:\tunknown\nState:\tR\nPid:\t0\n");
    }

    char state_ch = 'R';
    switch (curr->state) {
        case TASK_RUNNING: state_ch = 'R'; break;
        case TASK_INTERRUPTIBLE: state_ch = 'S'; break;
        case TASK_ZOMBIE: state_ch = 'Z'; break;
        default: state_ch = 'D'; break;
    }

    return snprintf(buf, max_len,
                    "Name:\t%s\n"
                    "State:\t%c (running)\n"
                    "Tgid:\t%d\n"
                    "Pid:\t%d\n"
                    "PPid:\t%d\n"
                    "Uid:\t%d\t%d\t%d\t%d\n"
                    "Gid:\t%d\t%d\t%d\t%d\n",
                    curr->comm, state_ch, curr->pid, curr->pid, curr->ppid,
                    curr->uid, curr->euid, curr->suid, curr->euid,
                    curr->gid, curr->egid, curr->sgid, curr->egid);
}

static ssize_t gen_self_cmdline(char *buf, size_t max_len)
{
    task_t *curr = sched_current();
    return snprintf(buf, max_len, "%s\n", curr ? curr->comm : "unknown");
}

static void create_proc_file(struct super_block *sb, struct dentry *parent, const char *name, proc_read_fn fn)
{
    struct inode *inode = inode_alloc(sb);
    if (!inode) return;

    inode->i_mode = S_IFREG | 0444;
    inode->i_fop = &proc_file_ops;

    struct proc_node *node = (struct proc_node *)kzalloc(sizeof(struct proc_node), GFP_KERNEL);
    if (node) {
        node->generator = fn;
        inode->i_private = node;
    }

    dentry_create(name, parent, inode);
}

struct super_block *procfs_create(void)
{
    struct super_block *sb = (struct super_block *)kzalloc(sizeof(struct super_block), GFP_KERNEL);
    if (!sb) {
        return NULL;
    }

    sb->s_blocksize = 1024;
    sb->s_type = "procfs";

    /* Root directory /proc */
    struct inode *root_inode = inode_alloc(sb);
    if (!root_inode) {
        kfree(sb);
        return NULL;
    }
    root_inode->i_mode = S_IFDIR | 0555;

    struct dentry *root_dentry = dentry_create("proc", NULL, root_inode);
    if (!root_dentry) {
        kfree(root_inode);
        kfree(sb);
        return NULL;
    }
    sb->s_root = root_dentry;

    /* Populate standard proc files */
    create_proc_file(sb, root_dentry, "meminfo", gen_meminfo);
    create_proc_file(sb, root_dentry, "cpuinfo", gen_cpuinfo);
    create_proc_file(sb, root_dentry, "version", gen_version);
    create_proc_file(sb, root_dentry, "uptime", gen_uptime);
    create_proc_file(sb, root_dentry, "loadavg", gen_loadavg);
    create_proc_file(sb, root_dentry, "cmdline", gen_cmdline);
    create_proc_file(sb, root_dentry, "filesystems", gen_filesystems);
    create_proc_file(sb, root_dentry, "devices", gen_devices);
    create_proc_file(sb, root_dentry, "partitions", gen_partitions);
    create_proc_file(sb, root_dentry, "mounts", gen_mounts);

    /* /proc/self directory */
    struct inode *self_dir_inode = inode_alloc(sb);
    if (self_dir_inode) {
        self_dir_inode->i_mode = S_IFDIR | 0555;
        struct dentry *self_dentry = dentry_create("self", root_dentry, self_dir_inode);
        if (self_dentry) {
            create_proc_file(sb, self_dentry, "status", gen_self_status);
            create_proc_file(sb, self_dentry, "cmdline", gen_self_cmdline);
        }
    }

    return sb;
}
