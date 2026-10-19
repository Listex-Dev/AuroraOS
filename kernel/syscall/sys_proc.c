#include <aurora/syscall.h>
#include <aurora/proc/sched.h>
#include <aurora/proc/binaries.h>
#include <aurora/proc/stack.h>
#include <aurora/elf64.h>
#include <aurora/signal.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/fs/vfs.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/arch/x86_64/pit.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/io.h>
#include <aurora/errno.h>

extern void fork_ret_trampoline(void);
extern void enter_usermode(uint64_t rip, uint64_t rsp);

int64_t sys_getpid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->pid : 0;
}

int64_t sys_getppid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->ppid : 0;
}

int64_t sys_getuid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->uid : 0;
}

int64_t sys_getgid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->gid : 0;
}

int64_t sys_geteuid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->euid : 0;
}

int64_t sys_getegid(void)
{
    task_t *curr = sched_current();
    return curr ? curr->egid : 0;
}

int64_t sys_umask(mode_t mask)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;
    mode_t old_mask = curr->umask;
    curr->umask = mask & 0777;
    return old_mask;
}

int64_t sys_setuid(uid_t uid)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;

    if (curr->euid == 0) {
        curr->uid = uid;
        curr->euid = uid;
        curr->suid = uid;
        return 0;
    } else {
        if (uid == curr->uid || uid == curr->suid) {
            curr->euid = uid;
            return 0;
        }
        return -EPERM;
    }
}

int64_t sys_setgid(gid_t gid)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;

    if (curr->euid == 0) {
        curr->gid = gid;
        curr->egid = gid;
        curr->sgid = gid;
        return 0;
    } else {
        if (gid == curr->gid || gid == curr->sgid) {
            curr->egid = gid;
            return 0;
        }
        return -EPERM;
    }
}

int64_t sys_setreuid(uid_t ruid, uid_t euid)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;

    if (curr->euid == 0) {
        if (ruid != (uid_t)-1) curr->uid = ruid;
        if (euid != (uid_t)-1) curr->euid = euid;
        if (ruid != (uid_t)-1 || (euid != (uid_t)-1 && euid != curr->uid)) {
            curr->suid = curr->euid;
        }
        return 0;
    }

    if (ruid != (uid_t)-1 && ruid != curr->uid && ruid != curr->euid) {
        return -EPERM;
    }
    if (euid != (uid_t)-1 && euid != curr->uid && euid != curr->euid && euid != curr->suid) {
        return -EPERM;
    }

    if (ruid != (uid_t)-1) curr->uid = ruid;
    if (euid != (uid_t)-1) curr->euid = euid;
    return 0;
}

int64_t sys_setregid(gid_t rgid, gid_t egid)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;

    if (curr->euid == 0) {
        if (rgid != (gid_t)-1) curr->gid = rgid;
        if (egid != (gid_t)-1) curr->egid = egid;
        if (rgid != (gid_t)-1 || (egid != (gid_t)-1 && egid != curr->gid)) {
            curr->sgid = curr->egid;
        }
        return 0;
    }

    if (rgid != (gid_t)-1 && rgid != curr->gid && rgid != curr->egid) {
        return -EPERM;
    }
    if (egid != (gid_t)-1 && egid != curr->gid && egid != curr->egid && egid != curr->sgid) {
        return -EPERM;
    }

    if (rgid != (gid_t)-1) curr->gid = rgid;
    if (egid != (gid_t)-1) curr->egid = egid;
    return 0;
}

int64_t sys_nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    if (!req) return -EFAULT;
    uint64_t ticks_to_wait = (req->tv_sec * 5) + (req->tv_nsec / 200000000ULL);
    if (ticks_to_wait == 0 && (req->tv_sec > 0 || req->tv_nsec > 0)) {
        ticks_to_wait = 1;
    }
    uint64_t target_ticks = pit_get_ticks() + ticks_to_wait;
    for (uint64_t i = 0; i < ticks_to_wait * 50; i++) {
        if (pit_get_ticks() >= target_ticks) break;
        sched_yield();
    }
    return 0;
}

int64_t sys_exit(int status)
{
    task_t *curr = sched_current();
    if (curr) {
        printk("<6>[SYSCALL] Process %s (PID %d) exited with status %d\n",
               curr->comm, curr->pid, status);
        if (curr->files) {
            files_free((struct files_struct *)curr->files);
            curr->files = NULL;
        }
    }
    sched_exit(status);
    return 0;
}

int64_t sys_fork(void)
{
    task_t *curr = sched_current();
    if (!curr || !curr->regs) {
        return -EAGAIN;
    }

    /* 1. Allocate child task struct */
    task_t *child = (task_t *)kzalloc(sizeof(task_t), GFP_KERNEL);
    if (!child) {
        return -ENOMEM;
    }

    /* 2. Allocate child kernel stack (16 KiB) */
    child->kstack = kmalloc(KERNEL_STACK_SIZE, GFP_KERNEL);
    if (!child->kstack) {
        kfree(child);
        return -ENOMEM;
    }

    /* 3. Clone user address space (PML4) */
    uint64_t *src_pml4 = curr->pml4 ? curr->pml4 : vmm_get_kernel_pml4();
    child->pml4 = vmm_clone_user_space(src_pml4);
    if (!child->pml4) {
        kfree(child->kstack);
        kfree(child);
        return -ENOMEM;
    }
    child->cr3 = vmm_virt_to_phys_direct(child->pml4);

    /* 4. Setup child process metadata */
    child->pid = sched_allocate_pid();
    child->ppid = curr->pid;
    child->pgid = curr->pgid;
    child->sid = curr->sid;
    child->tty = curr->tty;
    child->state = TASK_RUNNING;
    child->time_slice = DEFAULT_TIME_SLICE;
    child->parent = curr;
    child->uid = curr->uid;
    child->euid = curr->euid;
    child->suid = curr->suid;
    child->gid = curr->gid;
    child->egid = curr->egid;
    child->sgid = curr->sgid;
    child->umask = curr->umask;
    child->brk_start = curr->brk_start;
    child->brk_current = curr->brk_current;
    strncpy(child->comm, curr->comm, sizeof(child->comm) - 1);

    /* Clone file descriptor table and filesystem root/pwd */
    child->files = files_clone((struct files_struct *)curr->files);
    child->fs = fs_clone((struct fs_struct *)curr->fs);

    /* 5. Setup child kernel stack and pt_regs */
    struct pt_regs *child_regs = (struct pt_regs *)((uintptr_t)child->kstack + KERNEL_STACK_SIZE - sizeof(struct pt_regs));
    memcpy(child_regs, curr->regs, sizeof(struct pt_regs));
    child_regs->orig_rax = 0; /* In child process, fork() returns 0 */

    /* Setup stack frame for cpu_switch_to */
    uint64_t *sp = (uint64_t *)child_regs;
    *(--sp) = (uint64_t)fork_ret_trampoline; /* Return address for ret in cpu_switch_to */
    *(--sp) = 0; /* r15 */
    *(--sp) = 0; /* r14 */
    *(--sp) = 0; /* r13 */
    *(--sp) = 0; /* r12 */
    *(--sp) = 0; /* rbp */
    *(--sp) = 0; /* rbx */

    child->rsp = (uint64_t)sp;
    child->regs = child_regs;

    /* 6. Enqueue child task in scheduler */
    sched_add_task(child);

    printk("<6>[FORK] Parent PID %d ('%s') forked child PID %d\n",
           curr->pid, curr->comm, child->pid);

    /* 7. Return child PID to parent */
    return child->pid;
}

int64_t sys_execve(const char *filename, char *const argv[], char *const envp[])
{
    task_t *curr = sched_current();
    if (!curr || !filename) {
        return -EINVAL;
    }

    char kfilename[64];
    strncpy(kfilename, filename, sizeof(kfilename) - 1);
    kfilename[sizeof(kfilename) - 1] = '\0';

    /* Safely copy argv strings into kernel memory before address space change */
    char kargs[16][128];
    char *kargv[17];
    int argc = 0;
    if (argv) {
        while (argv[argc] != NULL && argc < 16) {
            strncpy(kargs[argc], argv[argc], sizeof(kargs[argc]) - 1);
            kargs[argc][sizeof(kargs[argc]) - 1] = '\0';
            kargv[argc] = kargs[argc];
            argc++;
        }
    }
    kargv[argc] = NULL;

    /* Safely copy envp strings into kernel memory */
    char kenvs[16][128];
    char *kenvp[17];
    int envc = 0;
    if (envp) {
        while (envp[envc] != NULL && envc < 16) {
            strncpy(kenvs[envc], envp[envc], sizeof(kenvs[envc]) - 1);
            kenvs[envc][sizeof(kenvs[envc]) - 1] = '\0';
            kenvp[envc] = kenvs[envc];
            envc++;
        }
    }
    kenvp[envc] = NULL;

    /* Locate binary: check VFS first, then builtin binaries table */
    void *vfs_elf_buf = NULL;
    size_t binary_size = 0;
    const void *elf_data = NULL;
    mode_t bin_mode = 0;
    uid_t bin_uid = 0;
    gid_t bin_gid = 0;
    bool bin_has_stat = false;

    int vfs_fd = sys_open(kfilename, O_RDONLY, 0);
    if (vfs_fd >= 0) {
        struct stat st;
        if (sys_fstat(vfs_fd, &st) == 0) {
            bin_mode = st.st_mode;
            bin_uid = st.st_uid;
            bin_gid = st.st_gid;
            bin_has_stat = true;
            if (st.st_size > 0 && st.st_size < 16 * 1024 * 1024) {
                vfs_elf_buf = kmalloc(st.st_size, GFP_KERNEL);
                if (vfs_elf_buf) {
                    ssize_t rd = sys_read(vfs_fd, vfs_elf_buf, st.st_size);
                    if (rd == (ssize_t)st.st_size) {
                        elf_data = vfs_elf_buf;
                        binary_size = st.st_size;
                    } else {
                        kfree(vfs_elf_buf);
                        vfs_elf_buf = NULL;
                    }
                }
            }
        }
        sys_close(vfs_fd);
    }

    if (!elf_data) {
        elf_data = find_builtin_binary(kfilename, &binary_size);
    }

    if (!elf_data) {
        printk("<4>[EXECVE] Binary not found: %s\n", kfilename);
        return -ENOENT;
    }

    /* 1. Create clean user address space */
    uintptr_t new_pml4_phys = pmm_alloc_page();
    if (!new_pml4_phys) {
        if (vfs_elf_buf) kfree(vfs_elf_buf);
        return -ENOMEM;
    }
    uint64_t *new_pml4 = (uint64_t *)vmm_phys_to_virt(new_pml4_phys);
    memset(new_pml4, 0, PAGE_SIZE);

    /* Share upper half kernel mappings */
    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        new_pml4[i] = k_pml4[i];
    }

    /* Switch to new PML4 */
    uint64_t new_cr3 = new_pml4_phys;
    write_cr3(new_cr3);

    /* 2. Load ELF binary into new PML4 */
    uint64_t entry_point = 0;
    uint64_t brk_start = 0;
    int ret = elf64_load_pml4(new_pml4, elf_data, binary_size, &entry_point, &brk_start);
    if (vfs_elf_buf) {
        kfree(vfs_elf_buf);
        vfs_elf_buf = NULL;
    }
    if (ret < 0) {
        printk("<3>[EXECVE] Failed to load ELF %s: %d\n", filename, ret);
        return ret;
    }

    /* 3. Allocate and map user stack (16 KiB = 4 pages) with NX (No-Execute) protection */
    #define USER_STACK_TOP 0x00007FFFF0000000ULL
    #define USER_STACK_PAGES 4
    for (uint64_t p = 0; p < USER_STACK_PAGES; p++) {
        uintptr_t stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            return -ENOMEM;
        }
        memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);
        vmm_map_page(new_pml4,
                     USER_STACK_TOP - (p + 1) * PAGE_SIZE,
                     stack_phys,
                     PTE_USER | PTE_WRITABLE | PTE_NX);
    }

    /* 4. Construct System V AMD64 ABI user stack */
    uint64_t user_sp = setup_user_stack(USER_STACK_TOP, argc, (char **)kargv, (char **)kenvp, entry_point);

    /* 5. Free previous user space if custom */
    if (curr->pml4 && curr->pml4 != k_pml4) {
        vmm_destroy_user_space(curr->pml4);
    }

    /* Close file descriptors marked with FD_CLOEXEC */
    if (curr->files) {
        struct files_struct *files = (struct files_struct *)curr->files;
        for (int i = 0; i < NR_OPEN; i++) {
            if (files->fd[i] && (files->fd_flags[i] & FD_CLOEXEC)) {
                vfs_close(files->fd[i]);
                files->fd[i] = NULL;
                files->fd_flags[i] = 0;
            }
        }
    }

    curr->pml4 = new_pml4;
    curr->cr3 = new_cr3;
    curr->brk_start = brk_start;
    curr->brk_current = brk_start;

    /* Extract basename for comm */
    const char *base = kfilename;
    for (const char *p = kfilename; *p; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    strncpy(curr->comm, base, sizeof(curr->comm) - 1);

    printk("<6>[EXECVE] Process PID %d executing '%s': entry=%p, rsp=%p, argc=%d\n",
           curr->pid, curr->comm, (void *)entry_point, (void *)user_sp, argc);

    /* Apply S_ISUID / S_ISGID credentials */
    if (bin_has_stat) {
        if (bin_mode & S_ISUID) {
            curr->euid = bin_uid;
            curr->suid = bin_uid;
        }
        if (bin_mode & S_ISGID) {
            curr->egid = bin_gid;
            curr->sgid = bin_gid;
        }
    }

    /* 7. Reset user registers to start executing the new program */
    if (curr->regs) {
        memset(curr->regs, 0, sizeof(struct pt_regs));
        curr->regs->rip = entry_point;
        curr->regs->rsp = user_sp;
        curr->regs->cs = 0x23;
        curr->regs->ss = 0x1B;
        curr->regs->rflags = 0x202; /* IF enabled */
        curr->regs->orig_rax = 0;
        return 0; /* Returns via syscall_return_to_user directly into entry_point */
    }

    enter_usermode(entry_point, user_sp);
    return 0;
}

int64_t sys_wait4(pid_t pid, int *status, int options, void *rusage)
{
    (void)rusage;
    task_t *curr = sched_current();
    if (!curr) {
        return -ESRCH;
    }

    while (1) {
        bool has_children = false;
        task_t *zombie = NULL;
        task_t *stopped = NULL;

        for (size_t i = 0; i < sched_num_tasks(); i++) {
            task_t *t = sched_get_task_at(i);
            if (!t || t->ppid != curr->pid) {
                continue;
            }

            if (pid > 0 && t->pid != pid) {
                continue;
            }
            if (pid == 0 && t->pgid != curr->pgid) {
                continue;
            }
            if (pid < -1 && t->pgid != -pid) {
                continue;
            }

            has_children = true;

            if (t->state == TASK_ZOMBIE) {
                zombie = t;
                break;
            }

            if ((options & WUNTRACED) && t->state == TASK_STOPPED) {
                stopped = t;
                break;
            }
        }

        if (!has_children) {
            return -ECHILD;
        }

        if (zombie) {
            pid_t reaped_pid = zombie->pid;
            int exit_status;
            if (zombie->exit_code >= 128) {
                exit_status = (zombie->exit_code - 128) & 0x7F;
            } else {
                exit_status = (zombie->exit_code & 0xFF) << 8;
            }

            if (status) {
                *status = exit_status;
            }

            printk("<6>[WAIT4] Parent PID %d reaped child PID %d (exit code %d)\n",
                   curr->pid, reaped_pid, zombie->exit_code);

            sched_remove_task(zombie);
            if (zombie->pml4 && zombie->pml4 != vmm_get_kernel_pml4()) {
                vmm_destroy_user_space(zombie->pml4);
            }
            task_destroy(zombie);

            return reaped_pid;
        }

        if (stopped) {
            if (status) {
                *status = ((stopped->exit_code >= 128 ? (stopped->exit_code - 128) : 20) << 8) | 0x7F;
            }
            return stopped->pid;
        }

        if (options & WNOHANG) {
            return 0;
        }

        /* Yield until child state changes */
        sched_yield();
    }
}

int64_t sys_kill(pid_t pid, int sig)
{
    if (sig < 0 || sig >= NSIG) {
        return -EINVAL;
    }

    if (pid < 0) {
        /* Signal process group */
        pid_t pgrp = -pid;
        int count = 0;
        for (size_t i = 0; i < sched_num_tasks(); i++) {
            task_t *t = sched_get_task_at(i);
            if (t && t->pgid == pgrp && t->state != TASK_ZOMBIE && t->state != TASK_DEAD) {
                if (sig > 0) {
                    printk("<6>[SIGNAL] Sent signal %d to PID %d ('%s', pgrp %d)\n",
                           sig, t->pid, t->comm, pgrp);
                    if (sig == SIGKILL || sig == SIGTERM || sig == SIGINT || sig == SIGQUIT || sig == SIGABRT) {
                        t->exit_code = 128 + sig;
                        t->state = TASK_ZOMBIE;
                        if (t->files) {
                            files_free((struct files_struct *)t->files);
                            t->files = NULL;
                        }
                    } else if (sig == SIGSTOP || sig == SIGTSTP) {
                        t->exit_code = 128 + sig;
                        t->state = TASK_STOPPED;
                    } else if (sig == SIGCONT) {
                        if (t->state == TASK_STOPPED) {
                            t->state = TASK_RUNNING;
                        }
                    }
                }
                count++;
            }
        }
        return count > 0 ? 0 : -ESRCH;
    }

    if (pid == 0) {
        task_t *curr = sched_current();
        if (!curr) return -ESRCH;
        return sys_kill(-(curr->pgid ? curr->pgid : curr->pid), sig);
    }

    task_t *target = sched_find_task(pid);
    if (!target) {
        return -ESRCH;
    }

    if (sig == 0) {
        return 0; /* Process existence probe */
    }

    printk("<6>[SIGNAL] Sent signal %d to PID %d ('%s')\n",
           sig, target->pid, target->comm);

    /* Default terminating signals: terminate target and set zombie status */
    if (sig == SIGKILL || sig == SIGTERM || sig == SIGINT || sig == SIGQUIT || sig == SIGABRT) {
        target->exit_code = 128 + sig;
        target->state = TASK_ZOMBIE;
        if (target->files) {
            files_free((struct files_struct *)target->files);
            target->files = NULL;
        }
        return 0;
    } else if (sig == SIGSTOP || sig == SIGTSTP) {
        target->exit_code = 128 + sig;
        target->state = TASK_STOPPED;
        return 0;
    } else if (sig == SIGCONT) {
        if (target->state == TASK_STOPPED) {
            target->state = TASK_RUNNING;
        }
        return 0;
    }

    return 0;
}

int64_t sys_setpgid(pid_t pid, pid_t pgid)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;

    if (pid == 0) pid = curr->pid;
    if (pgid == 0) pgid = pid;
    if (pgid < 0) return -EINVAL;

    task_t *target = (pid == curr->pid) ? curr : sched_find_task(pid);
    if (!target) return -ESRCH;

    target->pgid = pgid;
    return 0;
}

int64_t sys_getpgrp(void)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;
    return curr->pgid ? curr->pgid : curr->pid;
}

int64_t sys_getpgid(pid_t pid)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;
    if (pid == 0) return curr->pgid ? curr->pgid : curr->pid;

    task_t *target = sched_find_task(pid);
    if (!target) return -ESRCH;
    return target->pgid ? target->pgid : target->pid;
}

int64_t sys_setsid(void)
{
    task_t *curr = sched_current();
    if (!curr) return -ESRCH;

    if (curr->pgid == curr->pid && curr->sid == curr->pid) {
        return -EPERM;
    }

    curr->sid = curr->pid;
    curr->pgid = curr->pid;
    curr->tty = NULL;
    return curr->sid;
}

