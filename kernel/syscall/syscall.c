#include <aurora/syscall.h>
#include <aurora/arch/x86_64/msr.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/proc/sched.h>
#include <aurora/net/socket.h>
#include <aurora/errno.h>
#include <aurora/printk.h>

extern void syscall_entry(void);

void syscall_save_regs(struct pt_regs *regs)
{
    task_t *curr = sched_current();
    if (curr) {
        curr->regs = regs;
    }
}

/* Handler for int 0x80 compatibility */
static void int80_handler(interrupt_frame_t *frame)
{
    /* On int 0x80: RAX=num, RDI=a1, RSI=a2, RDX=a3, R10=a4, R8=a5, R9=a6 */
    int64_t ret = syscall_dispatch(frame->rax, frame->rdi, frame->rsi,
                                   frame->rdx, frame->r10, frame->r8, frame->r9);
    frame->rax = (uint64_t)ret;
}

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
{
    switch (num) {
        case SYS_read:
            return sys_read((int)a1, (void *)a2, (size_t)a3);
        case SYS_write:
            return sys_write((int)a1, (const void *)a2, (size_t)a3);
        case SYS_open:
            return sys_open((const char *)a1, (int)a2, (mode_t)a3);
        case SYS_close:
            return sys_close((int)a1);
        case SYS_stat:
            return sys_stat((const char *)a1, (struct stat *)a2);
        case SYS_fstat:
            return sys_fstat((int)a1, (struct stat *)a2);
        case SYS_lseek:
            return sys_lseek((int)a1, (uint64_t)a2, (int)a3);
        case SYS_ioctl:
            return sys_ioctl((int)a1, (unsigned long)a2, (void *)a3);
        case SYS_pipe:
            return sys_pipe((int *)a1);
        case SYS_dup:
            return sys_dup((int)a1);
        case SYS_dup2:
            return sys_dup2((int)a1, (int)a2);
        case SYS_nanosleep:
            return sys_nanosleep((const struct timespec *)a1, (struct timespec *)a2);
        case SYS_socket:
            return sys_socket((int)a1, (int)a2, (int)a3);
        case SYS_connect:
            return sys_connect((int)a1, (const struct sockaddr *)a2, (socklen_t)a3);
        case SYS_accept:
            return sys_accept((int)a1, (struct sockaddr *)a2, (socklen_t *)a3);
        case SYS_sendto:
            return sys_sendto((int)a1, (const void *)a2, (size_t)a3, (int)a4, (const struct sockaddr *)a5, (socklen_t)a6);
        case SYS_recvfrom:
            return sys_recvfrom((int)a1, (void *)a2, (size_t)a3, (int)a4, (struct sockaddr *)a5, (socklen_t *)a6);
        case SYS_shutdown:
            return sys_shutdown((int)a1, (int)a2);
        case SYS_bind:
            return sys_bind((int)a1, (const struct sockaddr *)a2, (socklen_t)a3);
        case SYS_listen:
            return sys_listen((int)a1, (int)a2);
        case SYS_getsockname:
            return sys_getsockname((int)a1, (struct sockaddr *)a2, (socklen_t *)a3);
        case SYS_getpeername:
            return sys_getpeername((int)a1, (struct sockaddr *)a2, (socklen_t *)a3);
        case SYS_setsockopt:
            return sys_setsockopt((int)a1, (int)a2, (int)a3, (const void *)a4, (socklen_t)a5);
        case SYS_getsockopt:
            return sys_getsockopt((int)a1, (int)a2, (int)a3, (void *)a4, (socklen_t *)a5);
        case SYS_mkdir:
            return sys_mkdir((const char *)a1, (mode_t)a2);
        case SYS_rmdir:
            return sys_unlink((const char *)a1);
        case SYS_unlink:
            return sys_unlink((const char *)a1);
        case SYS_exit:
            return sys_exit((int)a1);
        case SYS_getpid:
            return sys_getpid();
        case SYS_getppid:
            return sys_getppid();
        case SYS_getuid:
            return sys_getuid();
        case SYS_getgid:
            return sys_getgid();
        case SYS_syslog:
            return sys_syslog((int)a1, (char *)a2, (int)a3);
        case SYS_geteuid:
            return sys_geteuid();
        case SYS_getegid:
            return sys_getegid();
        case SYS_setuid:
            return sys_setuid((uid_t)a1);
        case SYS_setgid:
            return sys_setgid((gid_t)a1);
        case SYS_setpgid:
            return sys_setpgid((pid_t)a1, (pid_t)a2);
        case SYS_getpgrp:
            return sys_getpgrp();
        case SYS_setsid:
            return sys_setsid();
        case SYS_getpgid:
            return sys_getpgid((pid_t)a1);
        case SYS_uname:
            return sys_uname((struct utsname *)a1);
        case SYS_getcwd:
            return sys_getcwd((char *)a1, (size_t)a2);
        case SYS_chdir:
            return sys_chdir((const char *)a1);
        case SYS_getdents64:
            return sys_getdents64((int)a1, (void *)a2, (size_t)a3);
        case SYS_brk:
            return sys_brk((uintptr_t)a1);
        case SYS_fork:
            return sys_fork();
        case SYS_execve:
            return sys_execve((const char *)a1, (char *const *)a2, (char *const *)a3);
        case SYS_wait4:
            return sys_wait4((pid_t)a1, (int *)a2, (int)a3, (void *)a4);
        case SYS_kill:
            return sys_kill((pid_t)a1, (int)a2);
        case SYS_pivot_root:
            return sys_pivot_root((const char *)a1, (const char *)a2);
        case SYS_mount:
            return sys_mount((const char *)a1, (const char *)a2, (const char *)a3, (unsigned long)a4, (const void *)a5);
        case SYS_chmod:
            return sys_chmod((const char *)a1, (mode_t)a2);
        case SYS_fchmod:
            return sys_fchmod((int)a1, (mode_t)a2);
        case SYS_umask:
            return sys_umask((mode_t)a1);
        case SYS_setreuid:
            return sys_setreuid((uid_t)a1, (uid_t)a2);
        case SYS_setregid:
            return sys_setregid((gid_t)a1, (gid_t)a2);
        case SYS_arch_prctl:
            return sys_arch_prctl((int)a1, (unsigned long)a2);
        case SYS_mknod:
            return sys_mknod((const char *)a1, (mode_t)a2, (dev_t)a3);
        default:
            printk("<4>[SYSCALL] Unimplemented syscall %llu\n", (unsigned long long)num);
            return -ENOSYS;
    }
}

int64_t sys_arch_prctl(int code, unsigned long addr)
{
    task_t *curr = sched_current();
    if (code == 0x1002) { /* ARCH_SET_FS */
        if (curr) {
            curr->fs_base = addr;
        }
        wrmsr(MSR_FS_BASE, addr);
        return 0;
    } else if (code == 0x1003) { /* ARCH_GET_FS */
        if (!addr) return -EFAULT;
        *(uint64_t *)addr = curr ? curr->fs_base : rdmsr(MSR_FS_BASE);
        return 0;
    }
    return -EINVAL;
}

void syscall_init(void)
{
    /* 1. Enable SCE (bit 0) and NXE (bit 11) in EFER MSR */
    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | 1 | (1ULL << 11));

    /*
     * 2. Configure MSR_STAR:
     * Bits [47:32] = 0x08 (Kernel CS: sys ret uses CS=0x08, SS=0x10)
     * Bits [63:48] = 0x10 (User CS/SS base: sysret loads SS=0x1B, CS=0x23)
     */
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_STAR, star);

    /* 3. Configure MSR_LSTAR: target entry point */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. Configure MSR_SFMASK: clear IF (0x200), DF (0x400), TF (0x100) */
    wrmsr(MSR_SFMASK, 0x4700);

    /* 5. Register int 0x80 compatibility handler in IDT */
    isr_register_handler(0x80, int80_handler);

    printk("<6>[  0.000000] Syscall: MSR_LSTAR registered at %p (fast syscall and int 0x80 active)\n",
           (void *)syscall_entry);
}
