# AuroraOS Administrator & User Handbook

Welcome to **AuroraOS**, a 64-bit Unix-like operating system conforming to POSIX standards, featuring an ext4 root filesystem, preemptive multitasking, a full BSD TCP/IP networking stack, a SysV init service architecture, and cryptographic package management.

---

## 1. Booting & Login

### Bootstrapping
When booting from QEMU or real hardware:
```bash
make run
```
The kernel initializes memory, CPU subsystems, mounts the ext4 root filesystem, and launches `init` (PID 1).

`init` automatically spawns `getty` on the virtual console (`/dev/tty0`) and the serial console (`/dev/ttyS0`).

### User Accounts & Credentials
AuroraOS features multi-user security with shadow passwords:

| Username | Default Password | UID | Description |
|---|---|---|---|
| `root` | `root` | `0` | Superuser with unrestricted administrative access |
| `aurora` | `aurora` | `1000` | Standard unprivileged user account |

At the `aurora login:` prompt, enter your username and password.

### Switching Users & Changing Passwords
- Switch to another user:
  ```bash
  su aurora
  ```
- Switch to superuser (`root` password required):
  ```bash
  su -
  ```
- Verify your current identity:
  ```bash
  id
  ```

---

## 2. Shell & Terminal Environment

### Virtual Consoles
AuroraOS provides multiple virtual terminals (VT0 through VT5):
- Switch between consoles using **Alt + F1** through **Alt + F6**.
- Remote terminal emulation is available via PTYs (`/dev/pts/`).

### Shell Features (`/bin/sh`)
The system shell supports standard POSIX interactive capabilities:
- **Pipelines**:
  ```bash
  cat /etc/passwd | grep root
  ```
- **I/O Redirection**:
  ```bash
  echo "nameserver 10.0.2.3" > /etc/resolv.conf
  dmesg >> /var/log/dmesg.boot
  ```
- **Job Control & Signals**:
  - `Ctrl + C` sends `SIGINT` to terminate the active foreground process.
  - `Ctrl + Z` sends `SIGTSTP` to suspend execution.
  - Background processes using `&`:
    ```bash
    sleep 10 &
    ```
- **Scripting**:
  Scripts starting with `#!/bin/sh` are executable directly:
  ```bash
  chmod +x /etc/init.d/networking
  /etc/init.d/networking restart
  ```

---

## 3. Storage & Filesystem Administration

### Directory Hierarchy (FHS 3.0)
- `/bin`, `/sbin`: Core system binaries and administrator utilities.
- `/etc`: Configuration files (`passwd`, `shadow`, `fstab`, `inittab`, `init.d/`).
- `/dev`: Device nodes managed by `devtmpfs` (character and block devices).
- `/proc`, `/sys`: Kernel synthetic diagnostic and hardware interfaces.
- `/var`: Variable data, spool files, logs (`/var/log/`), and package DB (`/var/lib/pkg/`).
- `/usr`: Secondary hierarchy for user tools, documentation, and manual pages.
- `/tmp`: Temporary scratch directory.

### Disks and Partitions
- Inspect partition table:
  ```bash
  fdisk -l /dev/sda
  ```
- Mount and unmount partitions:
  ```bash
  mount /dev/sda2 /mnt
  ls -l /mnt
  umount /mnt
  ```
- File permissions and ownership:
  ```bash
  chmod 755 /usr/bin/myprog
  chown aurora:aurora /home/aurora/myfile
  ```

---

## 4. Network Configuration

### Automated Setup (DHCP)
To acquire an IP address automatically from the local DHCP server:
```bash
dhclient eth0
```

### Manual IP Configuration
Inspect or assign IP addresses manually:
```bash
# View network interface status
ifconfig

# Set static IP and netmask
ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up
```

### DNS & Connectivity
- Test network connectivity:
  ```bash
  ping 10.0.2.2
  ```
- Query domain names:
  ```bash
  nslookup example.com
  ```
- Download files via HTTP:
  ```bash
  wget http://10.0.2.2:8000/archive.tar
  ```
- Inspect active TCP/UDP sockets:
  ```bash
  netstat -an
  ```

### Secure Shell (SSH) Remote Access
AuroraOS runs an `sshd` daemon listening on port 22:
- Connect from the host computer:
  ```bash
  ssh aurora@localhost -p 2222
  ```
- Enter password `aurora`. The SSH session allocates a full pseudo-terminal (`pty`) supporting interactive terminal programs like `vi`.

---

## 5. System Services & SysV Init

The `init` daemon organizes system services through SysV runlevels:

| Runlevel | Purpose |
|---|---|
| `0` | System Halt |
| `1` | Single-user administrative recovery |
| `2` | Multi-user without networking |
| `3` | Full multi-user with networking, syslogd, crond, sshd (Default) |
| `6` | System Reboot |

### Managing Services
Service control scripts reside in `/etc/init.d/`:

```bash
/etc/init.d/syslogd {start|stop|restart|status}
/etc/init.d/crond   {start|stop|restart|status}
/etc/init.d/sshd    {start|stop|restart|status}
/etc/init.d/networking {start|stop|restart|status}
```

### Changing Runlevels
Switch runlevels dynamically using `telinit`:
```bash
telinit 6    # Reboots the machine cleanly
telinit 0    # Halts the system
```

---

## 6. Logging, Kernel Buffer & Cron

### System Logging
- Daemons log events via `syslog(3)` to the `syslogd` service.
- Logs are preserved in `/var/log/messages` and `/var/log/auth.log`.
- Send custom messages to syslog from shell scripts:
  ```bash
  logger -t myscript "Operation finished successfully"
  ```

### Kernel Message Buffer (`dmesg`)
Examine kernel boot messages and hardware driver status:
```bash
dmesg
```

### Periodic Task Scheduler (`crond`)
- View scheduled cron jobs for current user:
  ```bash
  crontab -l
  ```
- Edit user cron table:
  ```bash
  crontab -e
  ```
- Global system schedule is configured in `/etc/crontab`.

---

## 7. Package Management (`pkg`)

AuroraOS includes a package manager (`/bin/pkg`) with cryptographic HMAC-SHA256 signature verification:

```bash
# Install a signed package
pkg install /packages/sample.aurpkg

# List all installed packages
pkg list

# Display detailed package metadata
pkg info sample

# Remove an installed package
pkg remove sample

# Verify integrity and upgrade all system packages
pkg upgrade-system
```

---

## 8. Manual Pages & Documentation

System manuals are accessible directly on the console:

```bash
# Commands (Section 1)
man ls
man sh
man pkg
man dmesg

# File formats (Section 5)
man 5 passwd
man 5 shadow

# System administration (Section 8)
man 8 syslogd
man 8 crond
man 8 sshd
```

Additional documentation files are located in `/usr/share/doc/aurora/`:
- `HANDBOOK.txt`
- `ABI.txt`
- `PORTING.txt`
