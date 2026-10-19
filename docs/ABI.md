# AuroraOS Application Binary Interface (ABI) Specification

**Version:** 1.0.0  
**Target Architecture:** x86-64 (AMD64 / Intel 64)  
**Standard:** Linux x86-64 Calling Convention & Syscall ABI Compatible

---

## 1. System Call Gate Mechanism

AuroraOS supports both standard x86-64 syscall entry points:
1. **`syscall` (Fast Path, Preferred)**:
   - Utilizes MSR registers: `IA32_STAR`, `IA32_LSTAR` (`syscall_entry`), and `IA32_FMASK` (masks `IF`, `DF`, `TF`).
   - Clobbers `RCX` (stores user `RIP`) and `R11` (stores user `RFLAGS`).
2. **`int 0x80` (Legacy Path)**:
   - Invokes vector 128 through the IDT with DPL=3 for compatibility with classic toolchains.

### Register Calling Convention

| Argument | User Register | Kernel Function Parameter |
|---|---|---|
| **Syscall Number** | `RAX` | Selects handler in dispatch table |
| **Arg 1** | `RDI` | `a1` |
| **Arg 2** | `RSI` | `a2` |
| **Arg 3** | `RDX` | `a3` |
| **Arg 4** | `R10` *(note: RCX is clobbered by `syscall`)* | `a4` |
| **Arg 5** | `R8` | `a5` |
| **Arg 6** | `R9` | `a6` |
| **Return Value** | `RAX` | `>= 0` on success; `-errno` on failure |

**Callee-Preserved Registers:**  
`RBX`, `RBP`, `RSP`, `R12`, `R13`, `R14`, `R15`. The kernel guarantees that these registers retain their exact user values across system calls.

---

## 2. Process Memory Layout & Stack Initialization

### Virtual Address Map

```
0x00007FFFFFFFFFFF ──> Top of userspace addressable space
0x00007FFFFFFFE000 ──> Initial User Stack Pointer (RSP), grows DOWNWARDS (NX)
                         │
                         ▼
0x00000000XXXXXXXX ──> Program Break (brk), grows UPWARDS via sys_brk
0x000000000042XXXX ──> BSS & Data Segments (RW, NX)
0x0000000000400000 ──> Executable Text Segment (.text, RX)
0x0000000000000000 ──> Null Page Guard (Unmapped, prevents null pointer deref)
```

### Initial Stack Layout on `execve`

When a new ELF64 executable begins execution at `_start`, `RSP` points to the following stack layout:

```
[ High Memory ]
   String Data Area: argv strings, environment strings, executable path
   -----------------------------------------------------------------
   Null Auxiliary Vector Marker: AT_NULL (0, 0)
   Auxiliary Vector Entry 2:     { AT_PAGESZ, 4096 }
   Auxiliary Vector Entry 1:     { AT_ENTRY, entry_point }
   Auxiliary Vector Entry 0:     { AT_PHDR, phdr_address }
   NULL (terminates envp)
   envp[M-1] -> pointer to "PATH=/bin:/usr/bin"
   ...
   envp[0]   -> pointer to "USER=root"
   NULL (terminates argv)
   argv[N-1] -> pointer to argument string N-1
   ...
   argv[0]   -> pointer to program name string
   argc      -> 64-bit integer count of arguments
[ RSP ] ──────────────────────────────────────────────────────────
[ Low Memory ]
```

---

## 3. System Call Table

All syscall numbers match standard Linux x86-64 numbering:

| Number | Name | Signature | Description |
|---|---|---|---|
| `0` | `read` | `ssize_t read(int fd, void *buf, size_t count)` | Read from file descriptor |
| `1` | `write` | `ssize_t write(int fd, const void *buf, size_t count)` | Write to file descriptor |
| `2` | `open` | `int open(const char *filename, int flags, mode_t mode)` | Open or create file |
| `3` | `close` | `int close(int fd)` | Close open file descriptor |
| `4` | `stat` | `int stat(const char *filename, struct stat *statbuf)` | Get file status by path |
| `5` | `fstat` | `int fstat(int fd, struct stat *statbuf)` | Get file status by fd |
| `8` | `lseek` | `off_t lseek(int fd, off_t offset, int whence)` | Reposition file offset |
| `9` | `mmap` | `void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)` | Map pages into user space |
| `10` | `mprotect` | `int mprotect(void *addr, size_t len, int prot)` | Set memory page protections |
| `11` | `munmap` | `int munmap(void *addr, size_t len)` | Unmap pages from user space |
| `12` | `brk` | `void *brk(void *addr)` | Change data segment size (heap) |
| `13` | `rt_sigaction` | `int rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact, size_t sigsetsize)` | Configure signal handler |
| `14` | `rt_sigprocmask` | `int rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize)` | Examine and change blocked signals |
| `15` | `rt_sigreturn` | `int rt_sigreturn(void)` | Return from user signal trampoline |
| `16` | `ioctl` | `int ioctl(int fd, unsigned long req, ...)` | Control device parameters (termios/winsize) |
| `21` | `access` | `int access(const char *filename, int mode)` | Check user file permissions |
| `22` | `pipe` | `int pipe(int pipefd[2])` | Create unidirectional data pipe |
| `32` | `dup` | `int dup(int oldfd)` | Duplicate open file descriptor |
| `33` | `dup2` | `int dup2(int oldfd, int newfd)` | Duplicate fd to specific target number |
| `35` | `nanosleep` | `int nanosleep(const struct timespec *req, struct timespec *rem)` | High-resolution sleep |
| `39` | `getpid` | `pid_t getpid(void)` | Get calling process ID |
| `41` | `socket` | `int socket(int domain, int type, int protocol)` | Create communication endpoint |
| `42` | `connect` | `int connect(int fd, const struct sockaddr *addr, socklen_t len)` | Connect socket to remote peer |
| `43` | `accept` | `int accept(int fd, struct sockaddr *addr, socklen_t *len)` | Accept incoming connection |
| `44` | `sendto` | `ssize_t sendto(int fd, const void *buf, size_t len, int flags, ...)` | Send message on socket |
| `45` | `recvfrom` | `ssize_t recvfrom(int fd, void *buf, size_t len, int flags, ...)` | Receive message from socket |
| `46` | `sendmsg` | `ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)` | Send composite message on socket |
| `47` | `recvmsg` | `ssize_t recvmsg(int fd, struct msghdr *msg, int flags)` | Receive composite message on socket |
| `48` | `shutdown` | `int shutdown(int fd, int how)` | Shut down part of full-duplex socket |
| `49` | `bind` | `int bind(int fd, const struct sockaddr *addr, socklen_t len)` | Bind socket to local address |
| `50` | `listen` | `int listen(int fd, int backlog)` | Listen for socket connections |
| `51` | `getsockname` | `int getsockname(int fd, struct sockaddr *addr, socklen_t *len)` | Get current local socket address |
| `52` | `getpeername` | `int getpeername(int fd, struct sockaddr *addr, socklen_t *len)` | Get address of connected peer |
| `53` | `socketpair` | `int socketpair(int d, int type, int protocol, int sv[2])` | Create pair of connected sockets |
| `54` | `setsockopt` | `int setsockopt(int fd, int level, int optname, const void *val, socklen_t len)` | Set socket options |
| `55` | `getsockopt` | `int getsockopt(int fd, int level, int optname, void *val, socklen_t *len)` | Get socket options |
| `56` | `clone` | `int clone(unsigned long flags, void *child_stack, ...)` | Create thread or child process |
| `57` | `fork` | `pid_t fork(void)` | Create duplicate child process |
| `59` | `execve` | `int execve(const char *filename, char *const argv[], char *const envp[])` | Execute program |
| `60` | `exit` | `void exit(int error_code)` | Terminate calling process |
| `61` | `wait4` | `pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru)` | Wait for process state change |
| `62` | `kill` | `int kill(pid_t pid, int sig)` | Send signal to process |
| `63` | `uname` | `int uname(struct utsname *buf)` | Get operating system identification |
| `83` | `mkdir` | `int mkdir(const char *pathname, mode_t mode)` | Create directory |
| `84` | `rmdir` | `int rmdir(const char *pathname)` | Remove empty directory |
| `87` | `unlink` | `int unlink(const char *pathname)` | Delete file name from VFS |
| `90` | `chmod` | `int chmod(const char *filename, mode_t mode)` | Change file permission mode |
| `92` | `chown` | `int chown(const char *filename, uid_t user, gid_t group)` | Change file owner and group |
| `95` | `umask` | `mode_t umask(mode_t mask)` | Set file mode creation mask |
| `102` | `getuid` | `uid_t getuid(void)` | Get real user ID |
| `103` | `syslog` | `int syslog(int type, char *buf, int len)` | Read/control kernel log buffer (`klogctl`) |
| `104` | `getgid` | `gid_t getgid(void)` | Get real group ID |
| `105` | `setuid` | `int setuid(uid_t uid)` | Set real and effective user ID |
| `106` | `setgid` | `int setgid(gid_t gid)` | Set real and effective group ID |
| `107` | `geteuid` | `uid_t geteuid(void)` | Get effective user ID |
| `108` | `getegid` | `gid_t getegid(void)` | Get effective group ID |
| `165` | `mount` | `int mount(const char *dev, const char *dir, const char *type, unsigned long flags, const void *data)` | Mount filesystem |
| `166` | `umount2` | `int umount2(const char *target, int flags)` | Unmount filesystem |
| `169` | `reboot` | `int reboot(int magic1, int magic2, int cmd, void *arg)` | Reboot machine or halt system |
| `217` | `getdents64` | `int getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count)` | Read directory entries |
| `293` | `pipe2` | `int pipe2(int pipefd[2], int flags)` | Create pipe with `O_CLOEXEC`/`O_NONBLOCK` |

---

## 4. Signal Delivery & Stack Frames

When a signal is delivered to a process with a custom signal handler:
1. The kernel pushes a signal frame to the user stack containing the process's current register context (`struct ucontext`), `siginfo_t`, and a return trampoline pointing to `sys_rt_sigreturn` (15).
2. The user signal handler is executed in Ring 3.
3. Upon completion, the handler returns into the trampoline, invoking `sys_rt_sigreturn` to atomically restore the pre-signal register state.
