# AuroraOS Coding Style & Conventions

This document outlines the development conventions, code organization principles, and engineering invariants governing the **AuroraOS** codebase.

---

## 1. Core Principles

1. **Linux/Unix Philosophical Fidelity**: Never invent custom kernel abstractions when a proven standard exists (`inode`, `dentry`, `file_operations`, `major:minor`, `termios`, BSD sockets).
2. **Defensive Error Handling**: User input or userspace program misbehavior must **never** cause a kernel panic. Always validate user pointers before dereferencing and return negative `-errno` values (`-EFAULT`, `-EINVAL`, `-EBADF`, `-ENOMEM`).
3. **Monolithic Invariants**: Subsystems are modular in design but compiled directly into the kernel image for zero-overhead internal communication.

---

## 2. Formatting & Syntax

1. **Indentation**: 4 spaces per indentation level. Avoid mixing tabs and spaces.
2. **Line Length**: Soft limit of 100 characters; hard limit of 120 characters where readability is improved.
3. **Braces**:
   - Functions: opening brace on a new line:
     ```c
     int vfs_open(const char *path, int flags, mode_t mode)
     {
         if (!path)
             return -EINVAL;
         ...
     }
     ```
   - Control statements (`if`, `for`, `while`, `switch`): opening brace on the same line:
     ```c
     if (condition) {
         action();
     } else {
         alternative();
     }
     ```
4. **Spacing**:
   - One space after control keywords: `if (x)`, `for (int i = 0; ...)`, `while (running)`.
   - No space between function name and parenthesis: `kmalloc(size)`, `printf("format\n")`.
   - Pointer asterisk touches the variable name: `char *buf`, `struct task_struct *current`.

---

## 3. Naming Conventions

### Variables & Functions
All identifiers use lowercase `snake_case`. Subsystem prefixes are strictly mandatory for globally scoped functions and symbols:

| Subsystem | Prefix | Example |
|---|---|---|
| Physical Memory | `pmm_` | `pmm_alloc_page()`, `pmm_free_page()` |
| Virtual Memory | `vmm_` | `vmm_map_page()`, `vmm_unmap_page()` |
| Dynamic Heap | `heap_`, `kmalloc`, `kfree` | `kmalloc()`, `kfree()` |
| Task & Scheduling | `sched_`, `task_` | `sched_yield()`, `task_create()` |
| Virtual File System | `vfs_` | `vfs_mount()`, `vfs_read()`, `vfs_write()` |
| ext4 Filesystem | `ext4_` | `ext4_mount()`, `ext4_read_inode()` |
| Networking | `net_`, `tcp_`, `ip_`, `arp_` | `tcp_connect()`, `ip_send_packet()` |
| System Calls | `sys_` | `sys_read()`, `sys_fork()`, `sys_syslog()` |

### Constants & Macros
Use uppercase with underscores:
```c
#define PAGE_SIZE       4096
#define MAX_TASKS       1024
#define TCP_WINDOW_SIZE 65535
```

### Typedefs & Structures
Kernel internal structures are explicitly named with `struct`:
```c
struct task_struct;
struct inode;
struct dentry;
struct net_buf;
```
Userspace libc types use POSIX `_t` suffix: `pid_t`, `uid_t`, `gid_t`, `mode_t`, `off_t`, `size_t`.

---

## 4. Error Handling & Invariants

1. **Kernel Internal Functions**: Return integer error codes (`0` on success, `< 0` on failure, e.g. `-ENOENT`, `-ENOMEM`).
2. **Panic Usage**:
   - `panic()` is strictly reserved for unrecoverable boot failures (e.g. inability to allocate initial PML4 page table) or corrupt CPU execution states.
   - For all other situations, recover cleanly and propagate the error up the call stack.
3. **Pointers from Ring 3**:
   - Always verify user pointers before dereference.
   - User buffers must reside in lower-half canonical memory (`< 0x0000800000000000`). Accessing kernel space from userspace triggers `-EFAULT`.

---

## 5. Header Guards & Modularity

Every header file must include standard preprocessor guards matching its relative repository path:

```c
#ifndef _AURORA_NET_TCP_H
#define _AURORA_NET_TCP_H

#include <stdint.h>
#include <stddef.h>

/* declarations */

#endif /* _AURORA_NET_TCP_H */
```

---

## 6. Commit Message Guidelines

AuroraOS follows Conventional Commits:

- `feat:` Adds a new user-facing or kernel subsystem capability.
- `fix:` Fixes a bug or regressions in an existing component.
- `refactor:` Code restructuring without functional changes.
- `test:` Introduces or updates acceptance and unit test suites.
- `docs:` Documentation improvements and specification updates.
