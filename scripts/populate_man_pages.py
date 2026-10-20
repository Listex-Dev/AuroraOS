import os

PAGES = {
    "user/share/man/man1/ls.1": """NAME
    ls - list directory contents

SYNOPSIS
    ls [-l] [-a] [-h] [-R] [FILE]...

DESCRIPTION
    List information about the FILEs (the current directory by default).

OPTIONS
    -l     use a long listing format
    -a     do not ignore entries starting with .
    -h     with -l, print human readable sizes
    -R     list subdirectories recursively

SEE ALSO
    cat(1), stat(1)
""",
    "user/share/man/man1/cat.1": """NAME
    cat - concatenate files and print on the standard output

SYNOPSIS
    cat [FILE]...

DESCRIPTION
    Concatenate FILE(s) to standard output.

SEE ALSO
    cp(1), echo(1)
""",
    "user/share/man/man1/cp.1": """NAME
    cp - copy files and directories

SYNOPSIS
    cp [-r] SOURCE DEST

DESCRIPTION
    Copy SOURCE to DEST.

SEE ALSO
    cat(1), ls(1)
""",
    "user/share/man/man1/ps.1": """NAME
    ps - report a snapshot of the current processes

SYNOPSIS
    ps [aux]

DESCRIPTION
    ps displays information about active system processes.

SEE ALSO
    kill(1), top(1)
""",
    "user/share/man/man1/vi.1": """NAME
    vi - screen-oriented visual text editor

SYNOPSIS
    vi [FILE]

DESCRIPTION
    Full-screen modal text editor operating in raw terminal mode.

SEE ALSO
    cat(1), sh(1)
""",
    "user/share/man/man1/grep.1": """NAME
    grep - print lines matching a pattern

SYNOPSIS
    grep [-r] [-n] [-i] [-v] PATTERN [FILE]...

DESCRIPTION
    grep searches for PATTERN in each FILE.

SEE ALSO
    sed(1), sh(1)
""",
    "user/share/man/man1/sh.1": """NAME
    sh - command language interpreter with pipes and job control

SYNOPSIS
    sh [-c string] [file]

DESCRIPTION
    Standard Unix command language interpreter.

SEE ALSO
    init(8), login(1)
""",
    "user/share/man/man1/su.1": """NAME
    su - run a command with substitute user and group ID

SYNOPSIS
    su [USER]

DESCRIPTION
    su allows a user to become another user during a session.

SEE ALSO
    id(1), login(1)
""",
    "user/share/man/man1/chmod.1": """NAME
    chmod - change file mode bits

SYNOPSIS
    chmod MODE FILE...

DESCRIPTION
    chmod changes the file mode bits of each given file according to MODE.

SEE ALSO
    ls(1), stat(1)
""",
    "user/share/man/man1/pkg.1": """NAME
    pkg - AuroraOS package manager

SYNOPSIS
    pkg install <file.aurpkg>
    pkg remove <name>
    pkg list
    pkg info <name>
    pkg verify <file|name>

DESCRIPTION
    Manages binary packages with cryptographic signature verification.

SEE ALSO
    man(1)
""",
    "user/share/man/man1/man.1": """NAME
    man - format and display the on-line manual pages

SYNOPSIS
    man [section] command

DESCRIPTION
    man is the system's manual pager.

SEE ALSO
    pkg(1)
""",
    "user/share/man/man1/dmesg.1": """NAME
    dmesg - print or control the kernel ring buffer

SYNOPSIS
    dmesg

DESCRIPTION
    dmesg is used to examine or control the kernel ring buffer.

SEE ALSO
    syslogd(8)
""",
    "user/share/man/man5/passwd.5": """NAME
    passwd - the password file

DESCRIPTION
    /etc/passwd contains user account information.
    Format: username:password:UID:GID:GECOS:home:shell

SEE ALSO
    shadow(5), login(1), su(1)
""",
    "user/share/man/man5/fstab.5": """NAME
    fstab - static information about the filesystems

DESCRIPTION
    /etc/fstab contains file system mount descriptions.

SEE ALSO
    mount(8)
""",
    "user/share/man/man8/init.8": """NAME
    init - AuroraOS process control initialization (PID 1)

SYNOPSIS
    init [runlevel]

DESCRIPTION
    init is the parent of all processes on the system.

SEE ALSO
    telinit(8), sshd(8), syslogd(8)
""",
    "user/share/man/man8/syslogd.8": """NAME
    syslogd - system logging daemon

DESCRIPTION
    syslogd logs messages to /var/log/messages and /var/log/auth.log.

SEE ALSO
    logger(1)
""",
    "user/share/man/man8/sshd.8": """NAME
    sshd - AuroraOS remote shell daemon

DESCRIPTION
    sshd provides secure remote terminal access on TCP port 22.

SEE ALSO
    init(8), sh(1)
""",
    "user/share/man/man8/crond.8": """NAME
    crond - cron daemon for scheduled commands

DESCRIPTION
    crond executes scheduled jobs specified in /etc/crontab.

SEE ALSO
    crontab(1)
""",
    "user/share/man/man8/ifconfig.8": """NAME
    ifconfig - configure network interfaces

SYNOPSIS
    ifconfig [interface] [address] [up|down]

DESCRIPTION
    Configures kernel network interfaces (lo, eth0).

SEE ALSO
    netstat(1), ping(1)
""",
    "user/share/man/man8/insmod.8": """NAME
    insmod - simple program to insert a module into the AuroraOS kernel

SYNOPSIS
    insmod [filename.ko] [args...]

DESCRIPTION
    insmod is a user space utility to load a module into the kernel using init_module(2).

SEE ALSO
    rmmod(8), lsmod(8)
""",
    "user/share/man/man8/rmmod.8": """NAME
    rmmod - simple program to remove a module from the AuroraOS kernel

SYNOPSIS
    rmmod [modulename]

DESCRIPTION
    rmmod is a user space utility to unload a module from the kernel using delete_module(2).

SEE ALSO
    insmod(8), lsmod(8)
""",
    "user/share/man/man8/lsmod.8": """NAME
    lsmod - show the status of modules in the AuroraOS kernel

SYNOPSIS
    lsmod

DESCRIPTION
    lsmod is a program to show the status of modules by reading /proc/modules.

SEE ALSO
    insmod(8), rmmod(8)
""",
    "user/share/doc/aurora/HANDBOOK.txt": """AuroraOS 2.0 Ultra System Handbook
===================================

1. System Architecture
AuroraOS is a modern monolithic POSIX-compatible operating system for x86-64 long mode.
All system abstractions follow standard Unix principles: everything is a file,
virtual filesystems (/proc, /sys, /dev, /tmp), and ring-3 processes with fast syscall gate.

2. Process Management
Full fork/execve/wait4 lifecycle with signals, job control, and zombie reaping.

3. Loadable Kernel Modules (LKM)
Dynamic loading and unloading of kernel modules (.ko) with in-kernel ELF64 relocation.

4. Package Management
Managed via pkg with cryptographic HMAC-SHA256 signatures and /var/lib/pkg/db.

5. Services & Daemons
SysV service framework under /etc/init.d/ managing syslogd, crond, networking, and sshd.
""",
    "user/share/doc/aurora/ABI.txt": """AuroraOS System Call ABI Specification
======================================

Architecture: x86-64 (AMD64 Long Mode)
Instruction: syscall / sysretq

Registers:
  Syscall Number:  RAX
  Argument 1:      RDI
  Argument 2:      RSI
  Argument 3:      RDX
  Argument 4:      R10
  Argument 5:      R8
  Argument 6:      R9
  Return Value:    RAX (>=0 success, -errno on error)
""",
    "user/share/doc/aurora/PORTING.txt": """AuroraOS Application Porting Guide
==================================

1. Runtime:
AuroraOS libc provides POSIX unistd, stdio, stdlib, dirent, termios, signal, sha256.

2. Binary Format:
ELF64 static or dynamic executables loaded via ELF PT_LOAD segments.
"""
}

for rel_path, content in PAGES.items():
    full_path = os.path.join(os.getcwd(), rel_path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Created {rel_path}")
print("All manual pages and system documentation created.")
