# AuroraOS 1.0.0

[![Architecture](https://img.shields.io/badge/Architecture-x86--64%20(Long%20Mode)-blue.svg)](#)
[![Status](https://img.shields.io/badge/Status-v1.0.0%20Released-brightgreen.svg)](#)
[![Tests](https://img.shields.io/badge/Acceptance%20Tests-100%25%20Passing-success.svg)](#)
[![Compliance](https://img.shields.io/badge/Standards-POSIX.1--2017%20%7C%20FHS%203.0-orange.svg)](#)
[![License](https://img.shields.io/badge/License-MIT-purple.svg)](LICENSE)

**AuroraOS** is an independent, from-scratch 64-bit Unix-like operating system designed for the x86-64 architecture. It adheres to the classical Unix philosophy and standard Linux abstractions: Virtual File System (`inode`, `dentry`, `file_operations`), device nodes with `major:minor`, `/dev`, `/proc`, `/sys`, preemptive multitasking, BSD sockets, SysV init, and a cryptographically signed package management system.

---

## 🌟 Feature Highlights

- **Kernel Core**:
  - 64-bit Long Mode bootstrap with 4-level paging (`PML4` -> `PDPT` -> `PD` -> `PT`) and physical memory direct mapping.
  - Physical Memory Manager (bitmap allocator) & dynamic kernel heap with slab caches.
  - Preemptive multitasking scheduler (100 Hz PIT timer) with clean context switching and process isolation.
  - Complete POSIX process lifecycle: `fork()`, `execve()` (ELF64 loader), `wait4()`/`waitpid()`, signals, and process reaping.

- **Storage & VFS**:
  - Virtual File System supporting hierarchical mount tables and dentry path resolution.
  - **`ext4`** root filesystem driver with directory indexing, block allocation, and read/write support.
  - Synthetic filesystems: `ramfs` (initrd), `devtmpfs` (automatic `/dev` node population), `procfs`, `sysfs`, `tmpfs`, `pipefs`, and `sockfs`.
  - Storage block device drivers: ATA PIO and AHCI SATA controller support (`/dev/sda*`).

- **Networking**:
  - Full TCP/IP stack implemented from the wire up: Ethernet, ARP resolution cache, IPv4 routing, ICMP ping, UDP sockets, and TCP with dynamic sliding window and sequence tracking.
  - BSD socket API (`socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `setsockopt`).
  - Hardware drivers: Intel 82540EM (`e1000`) and VirtIO Network (`virtio-net`).
  - Network applications: `dhclient`, `nslookup`, `ping`, `ifconfig`, `netstat`, `wget`, and `sshd`.

- **Security & Multi-User Architecture**:
  - Multi-user credential system (`uid`, `gid`, `euid`, `egid`, `umask`, `setuid`).
  - Shadow password database (`/etc/shadow` mode `0640`) with SHA-512 password hashing.
  - Kernel and userspace hardening: Stack Smashing Protection (`-fstack-protector-strong`) and hardware No-Execute (`NX` page bit).

- **Terminal & Services**:
  - Line discipline with canonical and raw modes, VT0..VT5 virtual consoles, and PTY allocation.
  - Interactive `/bin/sh` shell featuring pipelines (`|`), I/O redirections (`>`, `<`, `>>`), background jobs (`&`), and script execution.
  - SysV `init` (PID 1) with runlevels 0..6, `/etc/inittab`, and `/etc/init.d/` daemon management.
  - Daemons: `syslogd` (system logging), `crond` (scheduled tasks), and `sshd` (remote terminal access).
  - Package manager: `/bin/pkg` managing signed `.aurpkg` packages with HMAC-SHA256 verification.
  - Online documentation: `man(1)` system and kernel message ring buffer inspection via `/bin/dmesg`.

---

## 🚀 Quick Start

### Prerequisites
- GCC cross-compiler (`x86_64-linux-gnu-gcc` or host 64-bit GCC)
- NASM assembler
- QEMU (`qemu-system-x86_64`)
- GNU Make

### Build and Launch

1. **Build Userland Utilities & Libc**:
   ```bash
   bash scripts/build_userland.sh
   ```

2. **Build AuroraOS Kernel Image**:
   ```bash
   make kernel
   ```

3. **Launch in QEMU**:
   ```bash
   make run
   ```

### Default Credentials

| User | Password | Role |
|---|---|---|
| `root` | `root` | Superuser / Administrator |
| `aurora` | `aurora` | Standard User |

---

## 🧪 Testing & Verification

AuroraOS includes comprehensive acceptance and regression test suites covering all milestones:

```bash
make test
```

The automated test runner executes within QEMU and verifies:
- **M0..M4**: Bootstrapping, CPU exceptions, PMM/VMM allocation, context switching, and syscall gates.
- **M5..M9**: Process lifecycle (`fork`/`exec`/`wait`), VFS file I/O, ext4 filesystem persistence, and TTY line discipline.
- **M10..M14**: Libc compliance, `/bin/sh` scripting, block devices (`fdisk`/`mount`), TCP/IP networking, DHCP, DNS, and `wget`.
- **M15..M17**: Shadow credentials, `su`, NX/canary traps, `syslogd`, `crond`, `sshd`, and signed `.aurpkg` package installation.
- **M18**: Libc crypto vectors (NIST SHA-256), `/bin/dmesg` ring buffer, documentation presence, and process stress loops.

---

## 📚 Documentation Index

- [SPECIFICATION.md](SPECIFICATION.md) — Complete authoritative architectural and functional specification.
- [docs/ARCH.md](docs/ARCH.md) — Kernel and system subsystem architecture.
- [docs/ABI.md](docs/ABI.md) — System call ABI, registers, ELF64 memory layout, and signal frames.
- [docs/PORTING.md](docs/PORTING.md) — Application porting guide, libc headers, and `.aurpkg` packaging.
- [docs/CODING_STYLE.md](docs/CODING_STYLE.md) — Coding conventions, formatting standards, and commit guidelines.
- [docs/HANDBOOK.md](docs/HANDBOOK.md) — Comprehensive user and system administrator handbook.
