# AuroraOS Architecture Overview

**AuroraOS** is a 64-bit Unix-like monolithic operating system written for the x86-64 architecture from scratch. It adheres strictly to POSIX.1-2017 standards, FHS 3.0 directory hierarchy, and standard Linux/Unix system abstractions (`inode`, `dentry`, `file_operations`, `major:minor`, `/dev`, `/proc`, `/sys`, `fork`/`exec`/`wait`, BSD sockets).

---

## 1. Hardware & CPU Initialization

```
[ BIOS / UEFI ]
       │
       ▼
 [ Multiboot 1/2 ] ──> boot/boot.S (32-bit Protected Mode)
                            │
                            ├─ Set up identity page tables (PML4)
                            ├─ Enable PAE, Long Mode (EFER.LME) & Paging
                            ├─ Load 64-bit GDT (Kernel Code 0x08, Data 0x10)
                            │
                            ▼
                     kernel/main.c (64-bit Long Mode)
```

1. **Bootstrapping**:
   - The bootloader loads `build/aurora.elf` via Multiboot 1 / Multiboot 2 specification.
   - [boot/boot.S](file:///d:/DevProject/AuroraOS/boot/boot.S) sets up a temporary bootstrap page table identity-mapping the lowest 1 GiB of physical RAM.
   - Transitions CPU into x86-64 Long Mode by enabling Physical Address Extension (`CR4.PAE = 1`), Long Mode Enable (`IA32_EFER.LME = 1`), and Paging (`CR0.PG = 1`).
   - Long jumps to 64-bit subsegment and passes Multiboot Information pointer to `kmain()`.

2. **GDT & TSS**:
   - The Global Descriptor Table defines 64-bit Kernel Code (`0x08`), Kernel Data (`0x10`), User Data (`0x18 | 3`), and User Code (`0x20 | 3`), followed by a 16-byte Task State Segment (TSS) descriptor.
   - The TSS stores the privilege level 0 stack pointer (`RSP0`) switched automatically on hardware interrupts, exceptions, and syscall transitions.

3. **IDT & Interrupts**:
   - Interrupt Descriptor Table handles 256 vectors:
     - Vectors `0..31`: CPU exceptions (#DE, #DB, #BP, #OF, #BR, #UD, #NM, #DF, #TS, #NP, #SS, #GP, #PF, #MF, #AC, #MC, #XM).
     - Vector `32` (IRQ0): PIT Timer (100 Hz).
     - Vector `33` (IRQ1): PS/2 Keyboard.
     - Vector `36` (IRQ4): Serial COM1 (115200 baud).
     - Vector `43` (IRQ11): PCI / Network (e1000 / virtio-net).
     - Vector `46` (IRQ14): ATA Primary Channel.
     - Vector `128` (`0x80`): Legacy Linux-compatible Syscall Gate.

---

## 2. Memory Management

```
+------------------------------------+ 0xFFFFFFFFFFFFFFFF
| Higher Half Kernel Virtual Memory  |
| - Direct Physical Map (PhysOffset) |
| - Kernel Heap (kmalloc/kfree)      |
| - Slab Caches                      |
+------------------------------------+ 0xFFFF800000000000
| Non-Canonical Gap                  |
+------------------------------------+ 0x00007FFFFFFFFFFF
| Userspace Memory:                  |
| - User Stack (grows down, NX)      |
| - Heap (brk)                       |
| - BSS & Data                       |
| - Code / Text (.text, RX)          |
+------------------------------------+ 0x0000000000400000
```

1. **Physical Memory Manager (PMM)**:
   - Uses a bitmap allocator tracking 4 KiB physical page frames parsed from the Multiboot memory map.
   - Manages reserved regions (kernel image, initrd, ACPI tables, MMIO) and provides `pmm_alloc_page()` / `pmm_free_page()`.

2. **Virtual Memory Manager (VMM)**:
   - 4-level paging (`PML4` -> `PDPT` -> `PD` -> `PT`).
   - Implements Direct Mapping: all physical RAM is mapped into higher-half virtual address space starting at `PHYS_MAP_BASE`.
   - Per-process page directories: each task owns an independent PML4 with lower-half userspace mappings (`0x0000000000400000` to `0x00007FFFFFFFFFFF`) while sharing kernel higher-half mappings.
   - Protection attributes: `PAGE_PRESENT`, `PAGE_WRITABLE`, `PAGE_USER`, and `PAGE_NX` (Execute-Disable bit 63).

3. **Kernel Heap & Slabs**:
   - Dynamic memory allocator (`kmalloc` / `kfree`) providing variable-sized memory blocks.
   - Specialized slab caches for hot kernel data structures (`task_struct`, `inode`, `dentry`, `file`, `socket`).

4. **Userspace Memory Layout**:
   - Base executable loaded at `0x0000000000400000`.
   - Program break (`brk`) expands the userspace heap dynamically.
   - Userspace stack is placed at `0x00007FFFFFFFE000` growing downwards, marked Non-Executable (`NX`) to prevent shellcode injection.

---

## 3. Process Management & Multitasking

```
                       +----------------+
                       |    CREATED     |
                       +-------┬--------+
                               │ sched_enqueue
                               ▼
        +─────────────>+----------------+<─────────────+
        │              |     READY      |              │
        │              +-------┬--------+              │
        │                      │ context_switch        │
        │                      ▼                       │
        │              +----------------+              │
        │ sched_yield  |    RUNNING     |              │
        +──────────────+-------┬--------+              │
                               │                       │
                +──────────────┴──────────────+        │
                │ sleep / waitqueue           │ exit   │ wakeup
                ▼                             ▼        │
        +----------------+            +----------------+
        |    BLOCKED     |            |     ZOMBIE     |
        +-------┬--------+            +-------┬--------+
                │                             │ wait4
                └─────────────────────────────┴─> REAPED
```

1. **Task Representation (`task_struct`)**:
   - PID, PPID, state (`TASK_RUNNING`, `TASK_INTERRUPTIBLE`, `TASK_UNINTERRUPTIBLE`, `TASK_ZOMBIE`, `TASK_DEAD`).
   - Registers (`struct pt_regs`), kernel stack (`RSP0`), userspace stack pointer, page directory (`cr3`).
   - Credentials: `uid`, `gid`, `euid`, `egid`, `umask`.
   - File descriptor table (`struct file *files[MAX_FILES]`).
   - Signal handling table: actions, pending mask, blocked mask.
   - Memory descriptors: `start_code`, `end_code`, `start_brk`, `brk`, `start_stack`.

2. **Preemptive Scheduler**:
   - PIT timer fires at 100 Hz (10 ms time slice).
   - Round-robin / priority-based preemptive scheduling via `schedule()`.
   - Context switching (`context.S`) saves and restores CPU registers (`RBX`, `RSP`, `RBP`, `R12`-`R15`, `CR3`).

3. **Process Lifecycle**:
   - `fork()`: clones the calling process, creates a new PML4, copies page tables with Copy-On-Write or page duplication, duplicates open file descriptors, assigns unique PID.
   - `execve()`: parses 64-bit ELF executable, validates ELF magic and ABI, tears down existing user address space, maps ELF segments (`LOAD`), initializes stack frame with `argc`, `argv`, `envp`, and ELF Auxiliary Vectors (`AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_PAGESZ`, `AT_ENTRY`), switches to Ring 3.
   - `wait4()` / `waitpid()`: reaps terminated children, returns exit status, cleans up zombie structures preventing process table leaks.

---

## 4. Virtual File System (VFS)

```
                       [ Syscall Layer: open / read / write / stat ]
                                           │
                                           ▼
                             [ VFS Inode & Dentry Cache ]
                                           │
        ┌──────────────┬───────────────────┼───────────────────┬──────────────┐
        ▼              ▼                   ▼                   ▼              ▼
     [ext4]         [ramfs]            [devtmpfs]           [procfs]       [sysfs]
  (Root / Disk)    (Initrd)          (/dev nodes)          (/proc/PID)    (/sys/class)
        │
        ▼
   [Block Dev]
 (ATA / AHCI /dev/sda)
```

1. **Core Abstractions**:
   - `struct inode`: stores file metadata, mode (`S_IFREG`, `S_IFDIR`, `S_IFCHR`, `S_IFBLK`, `S_IFIFO`, `S_IFLNK`), UID, GID, size, timestamps, block pointers.
   - `struct dentry`: directory tree node caching hierarchical paths and names, accelerating path resolution (`namei`).
   - `struct file`: represents an open file instance with file offset (`f_pos`), flags (`O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_APPEND`), and driver operations pointer.
   - `struct file_operations`: generic function pointer table (`read`, `write`, `open`, `close`, `lseek`, `ioctl`, `readdir`, `poll`).

2. **Filesystem Drivers**:
   - **`ext4`**: Root filesystem driver with standard superblock parsing, block group descriptors, inode tables, direct/indirect/extent blocks, directory entry parsing, and write support.
   - **`ramfs`**: In-memory filesystem used for early boot and `initrd` mounting.
   - **`devtmpfs`**: Populates `/dev` automatically with character and block device nodes reflecting major:minor numbers (e.g. `/dev/null` [1:3], `/dev/zero` [1:5], `/dev/tty0` [4:0], `/dev/sda` [8:0]).
   - **`procfs`**: Synthetic filesystem under `/proc` exposing kernel state (`/proc/meminfo`, `/proc/cpuinfo`, `/proc/uptime`, `/proc/<pid>/status`, `/proc/<pid>/cmdline`).
   - **`sysfs`**: Hierarchy under `/sys` exposing hardware topology, network interfaces (`/sys/class/net/eth0`), and block devices.
   - **`tmpfs`**: In-memory scratch storage for `/tmp` and `/run`.
   - **`pipefs` / `sockfs`**: Synthetic filesystems backing anonymous pipes (`pipe2`) and Unix/Internet sockets.

---

## 5. Network Subsystem

```
                    [ User Applications: wget / ping / sshd / nslookup ]
                                           │
                                           ▼
                            [ BSD Socket Syscall Layer ]
                               (socket, connect, listen, ...)
                                           │
                        ┌──────────────────┴──────────────────┐
                        ▼                                     ▼
                [ TCP Protocol ]                      [ UDP Protocol ]
           - 3-Way Handshake (SYN/ACK)             - Datagram sending/receiving
           - Sequence & ACK Tracking               - Checksum verification
           - Sliding Window Flow Control           - DNS query support
           - Teardown (FIN/ACK, TIME_WAIT)
                        └──────────────────┬──────────────────┘
                                           │
                                           ▼
                                    [ IPv4 Layer ]
                               - Packet routing & filtering
                               - Fragment handling & TTL
                               - ICMP Ping Echo & Reply
                                           │
                                           ▼
                                    [ ARP Engine ]
                               - Dynamic MAC resolution table
                               - ARP Request / Reply cache
                                           │
                                           ▼
                               [ Network Interface (netif) ]
                                           │
                        ┌──────────────────┴──────────────────┐
                        ▼                                     ▼
                 [ Intel e1000 ]                       [ VirtIO-Net ]
```

1. **Architecture**:
   - Packet processing engine built around `struct net_buf` descriptors.
   - Dedicated network worker thread handling incoming interrupt ring queues.

2. **Protocol Implementations**:
   - **Ethernet & ARP**: Decodes 802.3 frames, maintains ARP resolution table with broadcast query and expiration.
   - **IPv4**: Validates header checksums, handles local delivery vs forwarding, routes via default gateway.
   - **ICMP**: Generates ICMP Echo Replies for incoming pings; supports userspace ICMP sockets (`/bin/ping`).
   - **UDP**: Datagram multiplexing by port numbers; powers DHCP client and DNS resolver (`gethostbyname`).
   - **TCP**: Full transmission control protocol engine featuring:
     - 3-way handshake state machine (`SYN_SENT`, `SYN_RECEIVED`, `ESTABLISHED`).
     - Dynamic receive/transmit sliding windows preventing buffer overflows.
     - Sequence number tracking and ACK synthesis.
     - Clean connection teardown (`FIN_WAIT_1`, `FIN_WAIT_2`, `CLOSE_WAIT`, `LAST_ACK`, `TIME_WAIT`).
     - BSD socket API integration supporting blocking and non-blocking I/O.

3. **Hardware Drivers**:
   - **Intel 82540EM (e1000)**: PCI bus probing, TX/RX ring descriptor buffer setup, hardware checksum offload.
   - **VirtIO-Net**: Paravirtualized network driver utilizing virtqueues for minimal host virtualization overhead.

---

## 6. Security, DAC & Protection

1. **Discretionary Access Control (DAC)**:
   - File permission bits (`rwxrwxrwx`, `suid`, `sgid`, sticky bit).
   - Credential validation against real and effective IDs (`uid`, `gid`, `euid`, `egid`).
   - Passwords securely hashed with SHA-512 crypt in `/etc/shadow` (read-restricted mode `0640`).

2. **Kernel Hardening**:
   - **Stack Smashing Protection (Canaries)**: Kernel and userspace compiled with `-fstack-protector-strong`. Canary corruption immediately triggers `SIGABRT` / panic.
   - **No-Execute (NX / XD Bit)**: Data, heap, and user stack pages are mapped with `PAGE_NX` enabled in PML4/PT. Code execution on stack generates immediate `#PF` and process termination by `SIGSEGV`.
   - **Address Isolation**: Ring 3 user code cannot read or modify higher-half kernel memory pages.
