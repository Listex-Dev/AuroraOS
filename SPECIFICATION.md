Сохраняй как `SPECIFICATION.md` — это единый самодостаточный документ. Всё переписано под **AuroraOS**, статус — **чистый лист (greenfield, ничего не реализовано)**, и вся модель намеренно сделана **Linux/Unix-подобной**: VFS с inode/dentry/file_operations, device nodes с major/minor, `/dev` + `devtmpfs`, `/proc`, `/sys`, файловые дескрипторы, сигналы, TTY/pty, нумерация системных вызовов по Linux x86-64, FHS 3.0.

---

# AuroraOS — Спецификация операционной системы

| Поле | Значение |
|---|---|
| **Имя системы** | **AuroraOS** |
| **Документ** | `SPECIFICATION.md` |
| **Версия документа** | 1.0 |
| **Версия ОС (целевая)** | 1.0.0 |
| **Статус проекта** | 🟥 **GREENFIELD — ничего не реализовано. Всё пишется с нуля.** |
| **Тип** | Монолитное ядро, консольная (TTY-only), POSIX-совместимая |
| **Целевая архитектура** | x86-64 (long mode), обратная совместимость i386 |
| **Модель совместимости** | Linux/Unix: POSIX.1-2017, SUSv4, FHS 3.0, Linux-like ABI |
| **Владелец решения** | Технический комитет AuroraOS |

> **Ключевой принцип документа:** AuroraOS не изобретает собственные абстракции. Там, где существует устоявшаяся модель Linux/Unix — берётся она: `inode`, `dentry`, `file_operations`, `major:minor`, `fd 0/1/2`, `fork/exec/wait`, `SIG*`, `errno`, `/proc`, `/sys`, `/dev`, FHS. Отклонения допускаются только с обоснованием в разделе 21.

---

## 1. Назначение и принципы

### 1.1 Что такое AuroraOS

AuroraOS — полноценная операционная система для серверов, embedded-устройств, учебных проектов и энтузиастов низкоуровневой разработки. 

В версии 1.0 графическая оболочка **отсутствует**: единственный интерфейс — терминал (VT и/или последовательный порт). Это позволяет сосредоточиться на создании абсолютно стабильного, прозрачного и производительного ядра. Однако архитектура системы не накладывает жестких идеологических запретов на будущее: **если сообщество сформирует запрос**, в будущих мажорных версиях (v2.0+) возможно появление графического стека (например, на базе framebuffer, DRM/KMS или легковесного Wayland-композитора), разработанного как официальный модуль.

Система должна быть **рабочей целиком**: от включения питания до `pkg install` и `ssh` — без заглушек, без «потом доделаю».
### 1.2 Философия (обязательна к соблюдению)

AuroraOS следует классическим правилам Unix (McIlroy / Piper):

| Правило | Формулировка в AuroraOS |
|---|---|
| **Правило модульности** | Каждая подсистема — отдельный файл/каталог с узким интерфейсом |
| **Правило ясности** | Читаемость важнее хитрости; `panic()` с текстом причины обязателен |
| **Правило компонуемости** | Всё — файл; программы соединяются каналами `|` |
| **Правило разделения** | Механизм в ядре, политика в пользовательском пространстве (`init`, `pkg`, скрипты) |
| **Правило простоты** | Добавлять сложность только когда она доказано неизбежна |
| **Правило минимальной неожиданности** | Поведение совпадает с Linux: те же коды возврата, те же `errno`, те же семантики |
| **Правило восстановления** | Любая ошибка — это возвращаемый код, а не падение системы |
| **Правило расширения** | Интерфейсы версионируются; слом ABI — только с новой мажорной версией |

> **«Всё есть файл»:** процесс → `/proc/<pid>/`, устройство → `/dev/<name>`, сокет → `fd`, параметр ядра → `/sys/...`, терминал → `/dev/ttyN`. Нет исключений.

### 1.3 Цели v1.0

1. Загрузиться с диска (MBR/Multiboot2), перейти в long mode, смонтировать корень, запустить `init` (PID 1) и выдать **рабочий логин в терминал**.
2. Запустить **реальные POSIX-процессы** из ELF-файлов: `fork` → `execve` → `wait4`, с сигналами и корректным сбором зомби.
3. Предоставить **Linux-подобную VFS** с монтируемыми ФС: `ramfs`, `ext2/ext4`, `procfs`, `sysfs`, `devtmpfs`, `tmpfs`, `pipefs`.
4. Предоставить **устройства как файлы**: `char`/`block` device nodes, `major:minor`, авто-создание в `/dev` через `devtmpfs`.
5. Рабочий **TCP/IP стек** (Ethernet → ARP → IPv4 → ICMP/UDP/TCP) + Berkeley Sockets + `e1000`/`virtio-net`.
6. Полный набор **консольных утилит** и оболочка `/bin/sh` с перенаправлениями и конвейерами.
7. Многопользовательский режим, права, `su`, `pkg`, службы `syslogd`/`crond`/`sshd`.
8. **Всё это — одновременно и без заглушек.**

### 1.4 Не-цели v1.0 (явно вырезано)

| Не входит в v1.0 | Причина |
|---|---|
| Графика / GUI / Wayland / X | Консольная ОС по определению |
| SMP (многоядерность) | v1.1; ядро проектируется SMP-ready, но стартует UP |
| Виртуальные ФС `cgroup`, `bpf`, `selinux` | Сложность не окупается в v1.0 |
| `ext4` journal в режиме recovery | v1.0: `ext2`-совместимый R/W + extents; full journal — v1.1 |
| Звук, USB, PCMCIA, Wi-Fi | Не консольный серверный минимум |
| 32-битные ELF-процессы | v1.0 исполняет только ELF64 |
| `systemd` | Только SysV-стиль: `/etc/init.d/` + `/etc/rc.conf` |
| Контейнеры, namespaces | v2.0 |

---

## 2. Текущее состояние: нулевой цикл

> 🟥 **Ни один компонент AuroraOS не существует.** Репозиторий пуст. Нет загрузчика, нет ядра, нет libc, нет утилит, нет образа диска.

| Вопрос | Ответ |
|---|---|
| Строка кода ядра | ~2700 |
| Работающий загрузчик | ✅ (Multiboot 1/2 трамплин в Long Mode) |
| Работающее ядро | ✅ (M0..M3: GDT/IDT, PMM/VMM/Heap, PIT/Sched) |
| Работающая shell | ❌ |
| Образ диска | ❌ |
| Версия | `0.0.1-dev` |
| Первая веха | **M0 — «Hello, serial»** — ✅ ЗАВЕРШЕНА |
| Вторая веха | **M1 — GDT, IDT, исключения** — ✅ ЗАВЕРШЕНА |
| Третья веха | **M2 — PMM, VMM, Direct Map, Heap** — ✅ ЗАВЕРШЕНА |
| Четвёртая веха | **M3 — PIT 100Hz, Multitasking, Sched** — ✅ ЗАВЕРШЕНА |

Все пункты чек-листов в этом документе отмечены `[ ]` **по определению** — это план, а не отчёт.

---

## 3. Общая архитектура

### 3.1 Слои

```
┌───────────────────────────────────────────────────────────┐
│  Пользовательское пространство (ring 3)                   │
│  ┌─────────┐ ┌──────────┐ ────────┐ ────────┐           │
│  │ /bin/sh │ │ coreutils│ │  vi    │ │  pkg   │  ...      │
│  └────┬────┘ └────┬─────┘ └───┬────┘ └───┬────┘           │
│       └───────────┴─────────────┴──────────┘               │
│                        │                                    │
│                 ┌──────┴──────┐                             │
│                 │    libc     │  (syscall stubs, stdio,     │
│                 │             │   malloc, string, socket)    │
│                 └──────┬──────┘                             │
├────────────────────────┼──────────────────────────────────┤
│                 syscall gate (int 0x80 / syscall)          │
├────────────────────────┼──────────────────────────────────┤
│  Ядро (ring 0)        ▼                                    │
│  ┌────────┐ ┌────────┐ ┌─────────┐ ┌───────┐ ┌──────────┐  │
│  │  PMM   │ │  VMM   │ │ Process │ │  VFS  │ │   Net    │  │
│  │  HEAP  │ │ PAGING │ │ SCHED   │ │  FS   │ │  STACK   │  │
│  └────────┘ └────────┘ └─────────┘ └───────┘ ──────────┘  │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  DRIVERS: vga, serial, kbd, pit, ata/ahci, e1000,  │   │
│  │           virtio, rtc, pci, acpi                     │   │
│  └──────────────────────────────────────────────────────┘   │
├────────────────────────────────────────────────────────────┤
│  Multiboot2 / MBR bootloader  →  GRUB                      │
└────────────────────────────────────────────────────────────┘
```

### 3.2 Кольца и переходы

| Переход | Механизм |
|---|---|
| User → Kernel (вызов) | `syscall` (MSR `LSTAR`) — основной; `int 0x80` — совместимость |
| User → Kernel (исключение) | IDT, gate DPL=3 для `int 0x80` |
| Kernel → User (возврат) | `sysret` / `iretq` |
| Прерывания | APIC (v1.1) / 8259A PIC (v1.0), ISR → softirq-очередь |
| Таймер | PIT 100 Гц (v1.0), HPET/APIC timer (v1.1) |

**ABI системного вызова (как Linux x86-64):**

| Регистр | Назначение |
|---|---|
| `rax` | номер вызова → возвращаемое значение |
| `rdi, rsi, rdx, r10, r8, r9` | аргументы 1–6 |
| Возврат | `0..-4095` = `-errno`; значение ≥ 0 = успех |

### 3.3 Карта виртуальной памяти ядра (long mode)

| Диапазон | Назначение |
|---|---|
| `0x0000_0000_0000_0000` – `0x0000_7FFF_FFFF_FFFF` | пользовательское пространство (32 ТБ) |
| `0xFFFF_8000_0000_0000` – `0xFFFF_80FF_FFFF_FFFF` | прямое отображение физпамяти (direct map, 1 ТБ) |
| `0xFFFF_8100_0000_0000` – … | kernel heap (`kmalloc`) |
| `0xFFFF_8200_0000_0000` – … | vmalloc-область |
| `0xFFFF_FFFF_FFFF_0000` – `0xFFFF_FFFF_FFFF_FFFF` | per-CPU / fixmap |

Поддержка до **1 ТБ RAM**; PML4 → PDPT → PD → PT, 4 KiB страницы, опционально 2 MiB large pages для direct map.

### 3.4 Порядок загрузки (обязательный сценарий)

| # | Шаг | Результат |
|---|---|---|
| 0 | BIOS/UEFI(CSM) → MBR / GRUB читает `multiboot2_header` | контроль у загрузчика |
| 1 | GRUB грузит `boot/aurora.elf` + `boot/initrd.cpio`, передаёт mbi2 | 32-бит protected mode |
| 2 | `boot/`: GDT (64-бит дескрипторы), стек, включение PAE, `EFER.LME`, paging → **long mode** | ring 0, 64-бит |
| 3 | `kmain(mbi2*)`: разбор mbi2 (memmap, cmdline, initrd, framebuffer) | структура `bootinfo` |
| 4 | Serial `COM1 0x3F8 115200 8N1` — **первый вывод: `AuroraOS 1.0.0 booting...`** | отладка доступна |
| 5 | PMM: bitmap свободных страниц из `memmap` | аллокатор готов |
| 6 | VMM: начальные PML4, direct map, heap | память готова |
| 7 | IDT + PIC + PIT 100 Гц + `panic()`/`oops` обработчики | исключения обрабатываются |
| 8 | ACPI (RSDP→XSDT→MADT/FADT), PCI-сканирование | топология известна |
| 9 | Драйверы базового набора: `vga`, `serial`, `kbd`, `rtc`, `ata/ahci` | консоль жива |
| 10 | VFS + `ramfs`; распаковка `initrd.cpio` в корень | `/` существует |
| 11 | `devtmpfs` на `/dev`: создание узлов из таблицы драйверов | `/dev/console`, `/dev/tty0` |
| 12 | Монтирование `procfs`→`/proc`, `sysfs`→`/sys`, `tmpfs`→`/tmp`,`/run` | виртуальные ФС |
| 13 | Чтение `/etc/fstab`; монтирование `ext4` с корневого раздела, `pivot_root` | настоящая корневая ФС |
| 14 | Инициализация `exec`: запуск `/sbin/init` (ELF64) | **PID 1** |
| 15 | `init` → `/etc/rc.conf` → `getty` на `tty0` → **`aurora login:`** | пользователь внутри |

**Любой сбой на шагах 3–13 → `panic()` с выводом в serial и останом.** На шагах 14–15 → `init` печатает причину в консоль и переходит в аварийную shell-оболочку.

### 3.5 Картина после загрузки (дерево процессов)

```
init (PID 1, /sbin/init)
├── syslogd            (/etc/init.d/syslogd start)
├── crond
├── getty tty0  ──→ login ──→ /bin/sh          (локальная консоль)
├── getty ttyS0 ──→ login ──→ /bin/sh          (serial)
├── sshd  ──→ sshd: aurora [priv] ──→ sshd: aurora@pts/0 ──→ /bin/sh
└── (пользовательские процессы, дети shell)
```

---

## 4. Подсистема памяти

### 4.1 PMM (физический аллокатор)

- Алгоритм: **bitmap**, 1 бит на 4 KiB страницу; приоритет — нижние адреса (кэш-локальность), резерв зоны `0–1 MiB`.
- API: `pmm_alloc_page()`, `pmm_alloc_pages(n)`, `pmm_free_page(p)`, `pmm_free_pages(p,n)`, `pmm_stats()`.
- Зоны: `ZONE_DMA` (<16 MiB), `ZONE_NORMAL`, `ZONE_HIGH` (>4 GiB).
- Каждая страница несёт `struct page` в mem_map (flags: `PG_reserved`, `PG_slab`, `PG_anon`, `PG_file`, refcount).

### 4.2 VMM и paging

- 4-level paging, lazily-allocated уровни; CoW через `mprotect(RO)` + `#PF` handler.
- Обработчик `#PF`: `vm_area` lookup → demand zero page / CoW / file-backed / `SIGSEGV`.
- `struct vm_area`: `start, end, prot, flags, file, pgoff, next` — **копия модели Linux `vm_area_struct`**.
- `mmap`: `MAP_PRIVATE|MAP_SHARED|MAP_ANONYMOUS|MAP_FIXED|MAP_GROWSDOWN`.

### 4.3 Kernel heap

- Двухуровневый: **slab allocator** (кэш фиксированных размеров 16/32/64/128/256/512/1024/2048/4096) + ** segregated free-list** для крупных.
- API: `kmalloc(size, flags)`, `kzalloc`, `kfree`, `krealloc`, `kmalloc_aligned`.
- `flags`: `GFP_KERNEL`, `GFP_ATOMIC`, `GFP_ZERO`.
- Отладка: magic-паттерн за пределами блока, `KASAN`-подобный redzone в `CONFIG_AURORA_DEBUG`.

### 4.4 Пользовательская память

- `brk`/`sbrk` — классический data-сегмент (для `malloc` в libc).
- `mmap` — основной путь для больших выделений и `execve`.
- Пользовательский `malloc` в libc — **binlog/dlmalloc-подобный**, с `MALLOC_CHECK_`-режимом.

---

## 5. Процессы, планировщик, сигналы

### 5.1 `struct task_struct` (копия модели Linux, упрощённая)

```c
struct task_struct {
    pid_t pid, ppid, pgid, sid, tgid;
    uid_t uid, euid, suid;  gid_t gid, egid, sgid;
    int   state;                 /* TASK_RUNNING/INTERRUPTIBLE/UNINTERRUPTIBLE/
                                    STOPPED/ZOMBIE */
    struct mm_struct *mm;        /* адресное пространство */
    struct files_struct *files;  /* fd-таблица            */
    struct fs_struct *fs;        /* cwd, root              */
    struct signal_struct *sig;  /* обработчики, маска     */
    struct task_struct *parent, *children, *sibling;
    struct tty_struct *tty;
    uint64_t utime, stime; int nice;
    void *kstack; uint64_t rsp;  /* для context switch     */
    int exit_code;
    char comm[16];
};
```

### 5.2 Жизненный цикл

```
        fork()                execve()               exit()/signal
parent ────────→ child ─────────────────→ running ─────────────→ zombie
   │                │                                            │
   │                └── SIGCHLD → parent                        │
   └────────── wait4() ←──────────────────────────────────────--┘
                    (init репит сирот — супервизор зомби обязателен)
```

- `fork()` — CoW всего адресного пространства, дублирование fd-таблицы (общие `struct file` со счётчиком ссылок), наследование `cwd/root/signal handlers` (обработчики `SIG_IGN` сохраняются, остальные сбрасываются в `SIG_DFL`).
- `execve()` — разбор ELF64: проверка `e_machine == EM_X86_64`, отображение `PT_LOAD`, сборка стека `argc/argv/envp/auxv` (`AT_HWCAP`, `AT_PAGESZ`, `AT_UID`, `AT_EXECFD`, `AT_RANDOM`), установка `entry`.
- `exit()` — закрытие fd, освобождение `mm`, отправка `SIGCHLD` родителю, передача детей `init`.
- **Сироты и зомби:** `init` обязан крутить цикл `wait4(-1, ..., WNOHANG)` — иначе утечка PID.

### 5.3 Планировщик

- Модель: **CFS-подобный**, vruntime, `nice` → вес (`-20..19` → `1024..15`).
- Тики: PIT 100 Гц → `scheduler_tick()` → при превышении кванта — `need_resched`.
- Очереди: `runqueue[140]` (приоритетные классы) + bitmap активных классов → O(1) выбор.
- Политики: `SCHED_OTHER` (v1.0), `SCHED_FIFO`/`SCHED_RR` (v1.1).
- Контекстный переключатель: сохранение `rbp/rbx/r12–r15/rsp`, загрузка `rsp` следующего, переключение `CR3` при смене `mm`.

### 5.4 Сигналы (номера — как в Linux)

| № | Имя | Действие по умолчанию |
|---|---|---|
| 1 | `SIGHUP` | terminate |
| 2 | `SIGINT` | terminate (Ctrl-C) |
| 3 | `SIGQUIT` | core |
| 4 | `SIGILL` | core |
| 5 | `SIGTRAP` | core |
| 6 | `SIGABRT` | core |
| 7 | `SIGBUS` | core |
| 8 | `SIGFPE` | core |
| 9 | `SIGKILL` | terminate (**не блокируется**) |
| 10 | `SIGUSR1` | terminate |
| 11 | `SIGSEGV` | core |
| 12 | `SIGUSR2` | terminate |
| 13 | `SIGPIPE` | terminate |
| 14 | `SIGALRM` | terminate |
| 15 | `SIGTERM` | terminate |
| 17 | `SIGCHLD` | ignore |
| 18 | `SIGCONT` | continue |
| 19 | `SIGSTOP` | stop (**не блокируется**) |
| 20 | `SIGTSTP` | stop (Ctrl-Z) |
| 21 | `SIGTTIN` | stop |
| 22 | `SIGTTOU` | stop |
| 28 | `SIGWINCH` | ignore |
| 31 | `SIGSYS` | core |

API: `signal()`, `sigaction()`, `sigprocmask()`, `sigpending()`, `kill()`, `raise()`, `pause()`, `sigreturn`.
Доставка: на возврате из syscall/прерывания — «signal frame» на стек пользователя, `sa_restorer` → `sigreturn`.

### 5.5 Группы, сессии, управляющий терминал

- `setpgid()`, `getpgid()`, `setsid()`, `getpgrp()`.
- **Foreground process group** терминала получает `SIGINT`/`SIGQUIT`/`SIGTSTP`; фоновые — `SIGTTIN`/`SIGTTOU` при чтении/записи в TTY.
- Это обязательное поведение: без него `Ctrl-C` не будет работать корректно в конвейерах.

---

## 6. Системные вызовы

### 6.1 Точка входа

| Механизм | Номера | Когда |
|---|---|---|
| `syscall` + `LSTAR` | все | основной путь, ELF64 |
| `int 0x80` | все (совместимо) | отладка, legacy |

Таблица: `syscall_table[256]`, проверка `nr < NR_syscalls`, иначе `-ENOSYS`.

### 6.2 Нумерация — **идентична Linux x86-64** (`unistd_64.h`)

| № | Вызов | № | Вызов |
|---|---|---|---|
| 0 | `read` | 42 | `socket` |
| 1 | `write` | 43 | `connect` |
| 2 | `open` | 44 | `accept` |
| 3 | `close` | 47 | `sendmsg` |
| 4 | `stat` | 48 | `recvmsg` |
| 5 | `fstat` | 49 | `bind` |
| 6 | `lstat` | 50 | `listen` |
| 7 | `poll` | 51 | `getsockname` |
| 8 | `lseek` | 54 | `setsockopt` |
| 9 | `mmap` | 55 | `getsockopt` |
| 10 | `mprotect` | 56 | `clone` |
| 11 | `munmap` | 57 | `fork` |
| 12 | `brk` | 59 | `execve` |
| 13 | `rt_sigaction` | 60 | `exit` |
| 14 | `rt_sigprocmask` | 61 | `wait4` |
| 15 | `rt_sigreturn` | 62 | `kill` |
| 16 | `ioctl` | 63 | `uname` |
| 19 | `readv` | 72 | `fcntl` |
| 20 | `writev` | 74 | `fsync` |
| 21 | `access` | 78 | `getdents64` |
| 22 | `pipe` | 79 | `getcwd` |
| 23 | `select` | 80 | `chdir` |
| 24 | `sched_yield` | 82 | `rename` |
| 25 | `mremap` | 83 | `mkdir` |
| 32 | `dup` | 84 | `rmdir` |
| 33 | `dup2` | 86 | `link` |
| 35 | `nanosleep` | 87 | `unlink` |
| 36 | `getitimer` | 88 | `symlink` |
| 38 | `setitimer` | 89 | `readlink` |
| 39 | `getpid` | 90 | `chmod` |
| 40 | `sendfile` | 92 | `chown` |
| 41 | `socket`* | 95 | `umask` |
| — | — | 96 | `gettimeofday` |
| — | — | 99 | `sysinfo` |
| — | — | 102 | `getuid` |
| — | — | 104 | `getgid` |
| — | — | 105 | `setuid` |
| — | — | 107 | `geteuid` |
| — | — | 109 | `setpgid` |
| — | — | 110 | `getppid` |
| — | — | 112 | `setsid` |
| — | — | 113 | `setreuid` |
| — | — | 158 | `arch_prctl` (`ARCH_SET_FS`) |

\* канонический номер `socket` — 41; `sendto/recvfrom` — 44/45, `shutdown` — 48. Полный список — `include/aurora/syscall.h`, сгенерированный из `syscall.tbl`.

**Обязательный минимум v1.0:** `read, write, open, close, stat, fstat, lstat, lseek, mmap, mprotect, munmap, brk, rt_sigaction, rt_sigprocmask, rt_sigreturn, ioctl, readv, writev, access, pipe, select, sched_yield, dup, dup2, nanosleep, getpid, fork, execve, exit, wait4, kill, uname, fcntl, getdents64, getcwd, chdir, rename, mkdir, rmdir, link, unlink, symlink, readlink, chmod, chown, umask, gettimeofday, sysinfo, getuid, getgid, setuid, setgid, geteuid, setpgid, getppid, setsid, arch_prctl` + **весь socket-набор**.

### 6.3 `errno` — как в Linux

| Код | Имя | | Код | Имя |
|---|---|---|---|---|
| 1 | `EPERM` | | 20 | `ENOTDIR` |
| 2 | `ENOENT` | | 21 | `EISDIR` |
| 3 | `ESRCH` | | 22 | `EINVAL` |
| 4 | `EINTR` | | 23 | `ENFILE` |
| 5 | `EIO` | | 24 | `EMFILE` |
| 6 | `ENXIO` | | 25 | `ENOTTY` |
| 8 | `ENOEXEC` | | 28 | `ENOSPC` |
| 9 | `EBADF` | | 30 | `EROFS` |
| 10 | `ECHILD` | | 31 | `EMLINK` |
| 11 | `EAGAIN` | | 32 | `EPIPE` |
| 12 | `ENOMEM` | | 36 | `ENAMETOOLONG` |
| 13 | `EACCES` | | 38 | `ENOSYS` |
| 14 | `EFAULT` | | 39 | `ENOTEMPTY` |
| 16 | `EBUSY` | | 40 | `ELOOP` |
| 17 | `EEXIST` | | 95 | `EOPNOTSUPP` |
| 19 | `ENODEV` | | 110 | `ETIMEDOUT` |

`strerror()` в libc обязан покрывать весь список.

---

## 7. Файловая система

### 7.1 Объектная модель VFS (копия Linux)

```
             ┌─────────────┐
             │ super_block │  одна смонтированная ФС
             └──────┬──────┘
                    │ s_root
             ┌──────▼──────┐        ┌──────────────┐
             │   dentry    │───────→│    inode     │  metadata + op
             └──────┬──────┘        └──────┬───────┘
                    │                      │ i_op / i_fop
             ┌──────▼──────┐        ┌──────▼───────┐
             │    file     │───────→│ file_operations│ read/write/
             └─────────────┘        └───────────────┘ ioctl/mmap/readdir
```

| Объект | Обязательные поля |
|---|---|
| `super_block` | `s_op`, `s_root`, `s_fs_info`, `s_dev (major:minor)`, `s_flags (RDONLY/NOSUID/NOEXEC)` |
| `inode` | `i_ino`, `i_mode`, `i_uid`, `i_gid`, `i_nlink`, `i_size`, `i_atime/i_mtime/i_ctime`, `i_rdev`, `i_op`, `i_fop`, `i_count` |
| `dentry` | `d_name`, `d_parent`, `d_inode`, hash-кэш, `d_count` |
| `file` | `f_path`, `f_pos`, `f_flags`, `f_op`, `f_inode` |
| `file_operations` | `open, release, read, write, llseek, readdir, unlocked_ioctl, mmap, flush` |
| `inode_operations` | `lookup, create, mkdir, unlink, rmdir, rename, symlink, link, getattr, setattr` |
| `super_operations` | `alloc_inode, write_inode, put_inode, sync_fs, statfs` |

**Правило:** ни одна конкретная ФС не знает о другой; всё общение — через `*_operations`.

### 7.2 Типы файлов (`i_mode & S_IFMT`) — как в Unix

| Маска | Тип | Где живёт |
|---|---|---|
| `S_IFREG 0100000` | обычный | ext4, ramfs, tmpfs |
| `S_IFDIR 0040000` | каталог | все |
| `S_IFCHR 0020000` | **символьное устройство** | `devtmpfs` |
| `S_IFBLK 0060000` | **блочное устройство** | `devtmpfs` |
| `S_IFIFO 0010000` | **именованный канал (FIFO)** | ext4 + `mkfifo` |
| `S_IFLNK 0120000` | символическая ссылка | ext4, ramfs |
| `S_IFSOCK 0140000` | сокет | sockfs |

Права: `S_ISUID 04000`, `S_ISGID 02000`, `S_ISVTX 01000`, `rwx` × {owner, group, other}.

### 7.3 Файловые дескрипторы — полная Unix-семантика

- Таблица на процесс: `struct files_struct { fd[NR_OPEN=1024]; }`, наследование при `fork`, **`FD_CLOEXEC`** — закрытие при `execve`.
- **`0 = stdin`, `1 = stdout`, `2 = stderr`** — гарантированы `init`/`getty` для каждого процесса.
- `dup`, `dup2`, `fcntl(F_DUPFD/F_GETFD/F_SETFD/F_GETFL/F_SETFL/F_SETLK)`.
- **Каналы:** `pipe(fd[2])` → `pipefs`, не seek-able, `EPIPE` при записи в закрытый, `SIGPIPE` отправляется писателю.
- **`/dev/fd` → `/proc/self/fd`** — symlink-каталог, как в Linux.
- `select()`/`poll()` работают по fd любого типа (TTY, сокет, канал, файл) — это обязательное условие для `sh`, `ssh`, `crond`.

### 7.4 Монтирование

| Требование | Детали |
|---|---|
| `mount(2)` | `mount(source, target, fstype, flags, data)` |
| `umount(2)` | отказ при `EBUSY` |
| `/etc/fstab` | `device mountpoint fstype options dump pass` |
| `mount -a` | монтирует всё из fstab |
| Точки монтирования | `dentry` с `mnt` — стек монтирований, разрешение путей спускается вниз |
| `pivot_root` | переход с initrd на реальный корень |
| `MS_RDONLY / MS_NOSUID / MS_NOEXEC / MS_REMOUNT` | обязательны |
| `statfs` | `f_bsize, f_blocks, f_bfree, f_files, f_namelen` → `df` |

**Эталонный `/etc/fstab` v1.0:**

```
/dev/sda2   /       ext4   rw,relatime          0 1
/dev/sda1   /boot   ext2   rw,nodev,nosuid      0 2
proc        /proc   proc   defaults             0 0
sysfs       /sys    sysfs  defaults             0 0
devtmpfs    /dev    devtmpfs defaults           0 0
tmpfs       /run    tmpfs  defaults,size=16M    0 0
tmpfs       /tmp    tmpfs  defaults,nosuid,nodev 0 0
```

### 7.5 Конкретные файловые системы

| ФС | v1.0 | Назначение | Особенности |
|---|---|---|---|
| **ramfs** | ✅ | initrd, временный корень | нет диска, inode в RAM, `cpio`-распаковщик |
| **ext2/ext4** | ✅ R/W | **корневая ФС** | inode table, block groups, extent-деревья (compat), `mkfs`, `fsck`, `lost+found` |
| **procfs** | ✅ | `/proc` | см. 7.7 |
| **sysfs** | ✅ | `/sys` | объекты устройств/шин, атрибуты `0644` |
| **devtmpfs** | ✅ | `/dev` | ядро **само** создаёт узлы при регистрации драйвера |
| **tmpfs** | ✅ | `/tmp`, `/run` | RAM + swap-готовность |
| **pipefs** | ✅ | анонимные каналы | безымянные, видны в `/proc/<pid>/fd` |
| **sockfs** | ✅ | сокеты | `fstat` → `S_IFSOCK` |
| **iso9660** | ❌ v1.1 | live-носители | — |
| **nfs / 9p** | ❌ v1.2 | сеть-хранилища | — |

**Требование к `ext4`:** совместимость с `ext2` (суперблок `0x400`, magic `0xEF53`), поддержка `uninit_bg`, `extent` (flag `EXT4_EXTENTS_FL`), `dir_index` (htree) — желательно; полный journal `jbd2` — v1.1, в v1.0 допустим `data=writeback` без восстановления.

### 7.6 Дерево каталогов — FHS 3.0 (строго)

```
/
├── bin/            # /bin/sh, ls, cat, cp, mv, echo, grep, find, vi, ps, kill
├── boot/           # aurora.elf, initrd.cpio, grub/, grub.cfg
├── dev/            # devtmpfs: console, tty0..63, ttyS0, tty, ptmx, pts/,
│                   #   null, zero, full, random, urandom,
│                   #   sda, sda1.., ram0
├── etc/
│   ├── passwd      # root:x:0:0:root:/root:/bin/sh
│   ├── shadow      # root:$6$<salt>$<hash>:19000:0:99999:7:::
│   ├── group       # root:x:0:root  wheel:x:10:root
│   ├── fstab
│   ├── hostname    # aurora
│   ├── os-release  # ID=aurora, NAME="AuroraOS", VERSION="1.0"
│   ├── issue       # баннер перед login
│   ├── motd        # после логина
│   ├── shells      # /bin/sh
│   ├── nsswitch.conf
│   ├── rc.conf     # порядок запуска служб
│   ├── init.d/     # syslogd, crond, sshd, networking (start|stop|restart)
│   ├── network/    # interfaces, ifcfg-eth0
│   └── ssh/        # sshd_config, host keys
├── home/           # /home/<user> — по одному на пользователя
├── lib/            # libc.so, libc.a, ld-aurora.so
├── lib64/          # 64-битные версии (compat-ссылка на /lib)
├── media/  mnt/    # точки монтирования съёмных/временных
├── opt/            # стороннее ПО
├── proc/           # procfs
├── root/           # дом root
├── run/            # tmpfs: pid-файлы /run/syslogd.pid, /run/utmp
├── sbin/           # init, fsck, mkfs, mount, umount, ifconfig, getty, shutdown
├── srv/            # данные сервисов
├── sys/            # sysfs
├── tmp/            # tmpfs, sticky bit 1777
├── usr/
│   ├── bin/        # пользовательские утилиты
│   ├── sbin/
│   ├── lib/        # libm, libcrypto
│   ├── include/    # заголовки (для разработки на машине)
│   ├── share/man/  # man1/, man5/, man8/  → `man ls`
│   └── src/        # исходники AuroraOS
└── var/
    ├── log/        # messages, auth.log, wtmp
    ├── run/  spool/
    ├── cache/
    └── lib/pkg/    # база пакетов pkg
```

**Обязательные symlink-совместимости:** `/lib64 → /lib`, `/bin → /usr/bin` (или обратная — выбрать одну и зафиксировать), `/sbin → /usr/sbin`.

### 7.7 Что именно обязаны экспортировать `/proc` и `/sys`

**`/proc` (procfs):**

| Путь | Содержимое |
|---|---|
| `/proc/<pid>/` | каталог на каждый процесс |
| `/proc/<pid>/cmdline` | argv, NUL-разделитель |
| `/proc/<pid>/status` | `Name, State, Tgid, Pid, PPid, Uid, Gid, VmSize, VmRSS, Threads` |
| `/proc/<pid>/stat` | однострочные поля (как Linux) |
| `/proc/<pid>/exe` | symlink на исполняемый файл |
| `/proc/<pid>/cwd` | symlink на рабочий каталог |
| `/proc/<pid>/fd/<n>` | symlink на объект fd |
| `/proc/<pid>/maps` | `start-end perms offset dev inode path` |
| `/proc/<pid>/mem` | доступ через `ptrace`-проверку |
| `/proc/self/` | symlink на текущего |
| `/proc/meminfo` | `MemTotal, MemFree, MemAvailable, Buffers, Cached` |
| `/proc/cpuinfo` | `model name, cpu MHz, core id, flags` |
| `/proc/version` | `AuroraOS version 1.0.0 (gcc) #1 SMP ...` |
| `/proc/uptime` | `secs idle` |
| `/proc/loadavg` | `0.00 0.00 0.00 1/23 456` |
| `/proc/cmdline` | параметры ядра из mbi2 |
| `/proc/devices` | список зарегистрированных major |
| `/proc/partitions` | `major minor #blocks name` |
| `/proc/mounts` | активные монтирования (=`/etc/mtab`) |
| `/proc/net/dev`, `/proc/net/tcp`, `/proc/net/udp`, `/proc/net/route`, `/proc/net/arp` | статистика сети → `netstat`, `ifconfig` |
| `/proc/filesystems` | список ФС → `mount -t` |

**`/sys` (sysfs):**

| Путь | Содержимое |
|---|---|
| `/sys/devices/pci0000:00/0000:00:03.0/` | PCI-устройство: `vendor`, `device`, `class`, `driver` |
| `/sys/class/net/eth0/` | `address`, `mtu`, `statistics/{rx,tx}_bytes`, `flags`, `operstate` |
| `/sys/class/tty/tty0/` | терминалы |
| `/sys/block/sda/` | `size`, `removable`, `sda1/` |
| `/sys/kernel/` | `hostname`, `panic`, `version` |
| `/sys/module/` | загруженные подсистемы |

### 7.8 Устройства как файлы — обязательная модель

| Требование | Деталь |
|---|---|
| Типы | `S_IFCHR`, `S_IFBLK` |
| Идентификация | `i_rdev = MAJOR<<8 \| MINOR` (новое: `new_encode_dev` — фиксируем один формат и документируем) |
| Регистрация | `register_chrdev(major, name, &fops)`, `register_blkdev(major, name, &fops)` |
| Автосоздание | **`devtmpfs`**: при `register_*dev` ядро само создаёт `/dev/<name>` с правильным `mode` и `uid/gid` |
| Вручную | `mknod /dev/foo c 1 3` — обязана работать |
| Открытие | `open("/dev/sda1")` → `blkdev_open` → `sda_fops`; `ioctl` пробрасывается драйверу |
| Права | `/dev/null 0666`, `/dev/console 0600 root`, `/dev/sda 0660 root:disk`, `/dev/tty 0666 root:tty` |
| Группы | `root`, `wheel`, `tty`, `disk`, `audio`(пустая), `users` |

---

## 8. Драйверы и таблица устройств

### 8.1 Классы драйверов v1.0

| Драйвер | Класс | Обязанности |
|---|---|---|
| `serial` (8250/16550) | char, `4,64+` | `COM1 0x3F8`, 115200 8N1, прерывание, кольцевой буфер — **главный канал отладки** |
| `vga` / text console | char, `4,0..63` | 80×25 текст, палитра, **6 VT**, переключение `Alt+F1..F6` |
| `kbd` | (ввод в VT) | PS/2 набор 1, scancode→keycode, `Ctrl-C/Z`, LED |
| `pit` | — | 100 Гц тик |
| `rtc` | — | `CMOS 0x70/0x71`, `hwclock` |
| `pci` | — | конфигурационный цикл 0/1, BAR, поиск `e1000`/`virtio` |
| `ata`/`ahci` | block, `8,*` | PIO→DMA, LBA48, `/dev/sda` |
| `e1000` | net | PCI `8086:100E`, ring RX/TX, прерывания |
| `virtio-net` | net | PCI `1AF4:1000`, virtqueue |
| `ps2/8042` | — | контроллер клавиатуры |

### 8.2 Каноническая таблица device numbers (как в Linux)

| Устройство | Тип | Major | Minor | Mode | Владелец |
|---|---|---|---|---|---|
| `/dev/ram0` | block | 1 | 0 | 0660 | root:disk |
| `/dev/null` | char | 1 | 3 | 0666 | root:root |
| `/dev/zero` | char | 1 | 5 | 0666 | root:root |
| `/dev/full` | char | 1 | 7 | 0666 | root:root |
| `/dev/random` | char | 1 | 8 | 0666 | root:root |
| `/dev/urandom` | char | 1 | 9 | 0666 | root:root |
| `/dev/tty0`…`/dev/tty63` | char | 4 | 0…63 | 0620 | root:tty |
| `/dev/ttyS0` | char | 4 | 64 | 0660 | root:dialout |
| `/dev/tty` | char | 5 | 0 | 0666 | root:tty |
| `/dev/console` | char | 5 | 1 | 0600 | root:root |
| `/dev/ptmx` | char | 5 | 2 | 0666 | root:tty |
| `/dev/pts/N` | char | 128+ | N | 0620 | user:tty |
| `/dev/sda` | block | 8 | 0 | 0660 | root:disk |
| `/dev/sda1` | block | 8 | 1 | 0660 | root:disk |
| `/dev/sdb` | block | 8 | 16 | 0660 | root:disk |

`/proc/devices` после загрузки обязан выводить ровно этот набор.

---

## 9. Консоль, TTY, терминалы

### 9.1 Требование: полный `termios`

| Группа | Обязательное |
|---|---|
| Структура | `struct termios { c_iflag, c_oflag, c_cflag, c_lflag, c_cc[NCCS], speed }` |
| Режимы | **canonical** (построчно, редактирование `Backspace`/`Ctrl-U`/`Ctrl-W`) и **raw** (для `vi`) |
| Флаги | `ICANON, ECHO, ECHOFF, ISIG, IEXTEN, INTR/QUIT/SUSP, IXON, OPOST, ONLCR, CS8, CREAD` |
| ioctl | `TCGETS, TCSETS, TCGETA, TIOCGWINSZ, TIOCSWINSZ, TIOCGPGRP, TIOCSPGRP, TIOCSCTTY, TIOCNOTTY` |
| Утилиты | `stty`, `tput`, `reset` |

### 9.2 Состав TTY-подсистемы

```
  приложение (sh, vi, ssh)
        │ read/write/ioctl
  ┌─────▼──────┐
  │  tty line  │  canonical/raw,Signals INTR/QUT/SUSP,winch
  └─────┬──────┘
        │ ops
  ┌─────▼──────────────────────┐
  │  дисциплины:  vt0..vt5  │  serial0  │  pty pair  │
  └────────────────────────────┴─────────┴────────────┘
        │                         │            │
     vga text                 8250 UART    pty master/slave
```

### 9.3 Обязательное поведение

- **6 виртуальных терминалов** (`Alt+F1..F6`), каждый со своим `getty`.
- **`getty` → `login` → `/bin/sh`**: вывод `/etc/issue`, запрос `aurora login:`, пароль без эхо, вывод `/etc/motd`.
- **pty-пара**: `open("/dev/ptmx")` → `grantpt`/`unlockpt` → `/dev/pts/N`. Без этого **`ssh` невозможен** — значит, pty обязателен в v1.0.
- **Сигналы с клавиатуры:** `Ctrl-C → SIGINT` foreground pgid, `Ctrl-Z → SIGTSTP`, `Ctrl-D → EOF`, `Ctrl-D` в shell → выход.
- **`SIGWINCH`** при изменении размера (serial/pty).
- Экран: 80×25, прокрутка, курсор, `BS/BEL/CR/LF`, минимум ANSI: `ESC[2J`, `ESC[H`, `ESC[K`, `ESC[<n>;<m>H`, SGR цвета (`30-37`, `1` bold, `0` reset) — нужно для `ls --color`, `vi`, `man`.

---

## 10. Сеть

### 10.1 Стек (снизу вверх)

| Уровень | v1.0 | Детали |
|---|---|---|
| Driver | ✅ | `e1000`, `virtio-net`; ring buffers, NAPI-lite, `ndo_start_xmit`, `netif_rx` |
| Link | ✅ | Ethernet II, MAC, multicast-фильтр, `loopback` (`lo`) |
| ARP | ✅ | request/reply, таблица `/proc/net/arp`, timeout, gratuitous |
| IPv4 | ✅ | заголовки, checksum, фрагментация (reasm обязательна), TTL, `ICMP` |
| ICMP | ✅ | Echo request/reply (`ping`), Destination Unreachable, Time Exceeded |
| UDP | ✅ | сокет-демультиплекс по порту, checksum (опц. проверять) |
| TCP | ✅ | **полная конечная машина**: 3-way handshake, sequence/ack, window, retransmit (RTO), fast retransmit, delayed ACK, `FIN`/`RST`, `TIME_WAIT`, keepalive |
| Сокеты | ✅ | `AF_INET`, `SOCK_STREAM`, `SOCK_DGRAM`, `SOCK_RAW` |
| Службы | ✅ | **DHCP-клиент** (DISCOVER/OFFER/REQUEST/ACK), **DNS-резолвер** (`/etc/resolv.conf`), `getaddrinfo` |

### 10.2 Socket API

```c
int  socket(int domain, int type, int protocol);
int  bind(int, const struct sockaddr *, socklen_t);
int  listen(int, int backlog);
int  accept(int, struct sockaddr *, socklen_t *);
int  connect(int, const struct sockaddr *, socklen_t);
ssize_t send(int, const void *, size_t, int flags);
ssize_t recv(int, void *, size_t, int flags);
ssize_t sendto / recvfrom / sendmsg / recvmsg;
int  shutdown(int, how);
int  setsockopt / getsockopt;      /* SO_REUSEADDR, SO_RCVBUF, SO_SNDBUF,
                                      TCP_NODELAY, SO_KEEPALIVE, SO_ERROR */
int  getsockname / getpeername;
int  socketpair(AF_UNIX, ...);     /* обязателен для ssh/pkg */
```

**Интеграция с VFS:** сокет — это `fd` типа `S_IFSOCK`; `read/write` == `recv/send`; `ioctl(FIONREAD)`; `select/poll` работает. **Сокет — файл.**

### 10.3 Конфигурация

```
/etc/network/interfaces:
  auto lo
  iface lo inet loopback
  auto eth0
  iface eth0 inet dhcp        # или static: address/netmask/gateway
/etc/hosts:
  127.0.0.1   localhost aurora
  ::1         localhost
/etc/resolv.conf:
  nameserver 10.0.2.3
  search lan
```

Утилиты: `ifconfig`, `ip` (упрощённый), `route`, `netstat`, `ping`, `nslookup`, `wget`, `ssh`/`sshd`.

---

## 11. Пользовательское пространство

### 11.1 libc (`/lib/libc.so`, статика `libc.a`)

| Заголовок | Обязательный набор |
|---|---|
| `unistd.h` | `read, write, open, close, lseek, fork, execve, _exit, wait, getpid, getppid, getuid, setuid, getcwd, chdir, pipe, dup, dup2, access, unlink, rmdir, mkdir, chown, chmod, symlink, readlink, alarm, sleep, usleep, sbrk, brk, isatty, ttyname, gethostname, sethostname, getlogin, syscall` |
| `stdio.h` | `printf, fprintf, sprintf, snprintf, vprintf, fopen, fclose, fread, fwrite, fgetc, fputc, fgets, fputs, fseek, ftell, rewind, fflush, perror, puts, putchar, getline, tmpfile, fdopen` |
| `stdlib.h` | `malloc, calloc, realloc, free, atoi, atol, strtol, strtoul, strtod, exit, _Exit, abort, atexit, getenv, setenv, unsetenv, system, qsort, bsearch, rand, srand, abs, div, mkstemp` |
| `string.h` | `strlen, strcpy, strncpy, strcat, strncat, strcmp, strncmp, strchr, strrchr, strstr, strtok, strdup, memcpy, memmove, memset, memcmp, memchr` |
| `fcntl.h` | `open, openat, creat, fcntl, O_*`, `S_*` |
| `sys/stat.h` | `stat, fstat, lstat, mkdir, chmod, umask`, все `S_IS*` макросы |
| `sys/types.h` | `pid_t, uid_t, gid_t, off_t, size_t, ssize_t, mode_t, dev_t, ino_t` |
| `sys/wait.h` | `wait, waitpid, wait4`, `WIFEXITED/WEXITSTATUS/WIFSIGNALED/WTERMSIG` |
| `signal.h` | полный набор из 5.4 |
| `sys/socket.h`, `netinet/*`, `arpa/inet.h` | сокеты, `htons/ntohl/inet_pton/inet_ntop` |
| `dirent.h` | `opendir, readdir, closedir, DIR, struct dirent` |
| `termios.h` | из раздела 9 |
| `time.h` | `time, gettimeofday, clock, localtime, gmtime, strftime, mktime` |
| `errno.h` | из 6.3 + `strerror` |
| `pwd.h`, `grp.h` | `getpwnam, getpwuid, getgrnam` — чтение `/etc/passwd`, `/etc/group` |
| `setjmp.h`, `assert.h`, `limits.h`, `stdarg.h` | обязательно |

**Требование:** libc — **единственный** пользовательский способ попасть в ядро. Никаких `int 0x80` в приложениях.

### 11.2 C-runtime и формат ELF

| Требование | Деталь |
|---|---|
| Формат | **ELF64**, `EM_X86_64`, `ET_EXEC` (PIE — v1.1) |
| Загрузка | `execve` → `PT_LOAD` отображения, `PT_INTERP` игнорируется в v1.0 (статическая линковка приоритетна) |
| Стек | `argc, argv[], NULL, envp[], NULL, auxv[]`, 16-байтное выравнивание на входе в `_start` |
| `_start` | `crt0`: инициализация `environ`, `argc/argv`, вызов `main`, `exit(main())` |
| Инициализация | `.bss` обнуляется ядром; `__libc_init` регистрирует `atexit`, ставит `stdin/out/err` |

### 11.3 Оболочка `/bin/sh` — POSIX-совместимая

**Обязательный синтаксис:**

```
команда [аргумент...]
команда1 | команда2 | команда3        # конвейер
команда > файл   >> файл   < файл     # перенаправление
команда &                             # фон
VAR=value; VAR=value команда           # окружение
$VAR   ${VAR}   $?   $$   $!   $0      # подстановки
if / then / fi;  while;  for;  case;   # управление
{ ...; };  ( ... )                     # группы и подshell
'одинарные'  "двойные"  \экранирование # кавычки
;  &&  ||                              # последовательность
*  ?  [a-z]                            # глоб (pathname expansion)
~                                      # home
# комментарий
```

**Встроенные команды (builtin):** `cd`, `echo`, `pwd`, `export`, `unset`, `set`, `shift`, `read`, `eval`, `exec`, `exit`, `return`, `test`/`[`, `true`, `false`, `:`, `break`, `continue`, `trap`, `alias`, `history`, `jobs`, `fg`, `bg`, `wait`, `type`, `which`, `help`, `clear`, `umask`.

**Внешние:** всё из `/bin`, `/usr/bin` через `execvp` с поиском по `$PATH`.

**Job control:** обязателен — `&`, `fg`, `bg`, `Ctrl-Z`, отчёт `[1]+ Stopped`. Без него оболочка не считается POSIX-ной.

### 11.4 Утилиты v1.0 (полный список)

| Категория | Утилиты |
|---|---|
| **Файлы** | `ls` (`-l -a -h -R --color`), `cat`, `cp` (`-r`), `mv`, `rm` (`-r -f`), `mkdir`, `rmdir`, `ln` (`-s`), `touch`, `stat`, `file`, `wc`, `head`, `tail`, `sort`, `uniq`, `cut`, `tr`, `tee`, `dd` |
| **Поиск/текст** | `grep` (`-r -n -i -v`), `find`, `sed` (минимум), `awk` (минимум), `vi` (полноэкранный, raw-режим), `man` |
| **Процессы** | `ps` (`aux`), `kill` (`-9`), `pkill`, `top` (простой), `nice`, `uptime`, `killall` |
| **Система** | `uname` (`-a`), `hostname`, `date`, `uptime`, `df`, `du`, `free`, `mount`, `umount`, `fdisk`, `mkfs`, `fsck`, `mknod`, `chown`, `chmod`, `id`, `whoami`, `su`, `login`, `getty`, `passwd`, `shutdow`/`reboot`, `dmesg` |
| **Сеть** | `ifconfig`, `ip`, `route`, `netstat`, `ping`, `nslookup`, `wget`, `ssh`, `scp`, `telnet` |
| **Пакеты** | `pkg` (`install, remove, update, upgrade-system, list, info, verify`) |
| **Прочее** | `stty`, `tput`, `clear`, `yes`, `seq`, `expr`, `basename`, `dirname`, `sleep`, `true`, `false`, `test`, `crontab` |

> **Правило:** каждая утилита — отдельный ELF, отдельный `man`-страница, отдельный исходник в `user/`. Никаких «гигантских» бинарников, кроме опционального `busybox`-режима.

### 11.5 Документация

- `man` + каталоги `man1/` (пользовательские), `man5/` (форматы файлов: `passwd(5)`, `fstab(5)`, `group(5)`), `man8/` (административные).
- Формат: `troff`-подобный подмножество → `man ls`, `man 5 passwd`.
- Дополнительно: `/usr/share/doc/aurora/{HANDBOOK,PORTING,ABI}.txt`.

---

## 12. Init и системные службы

### 12.1 `/sbin/init` (PID 1)

| Обязанность | Деталь |
|---|---|
| Монтирование | `mount -a` по `/etc/fstab`, создание `/dev/pts`, `/run` |
| Системный набор | установка hostname, `umask 022`, `PATH`, `TERM=linux` |
| Запуск служб | последовательно `/etc/init.d/<служба> start` в порядке `/etc/rc.conf` |
|getty | запуск `getty` на `tty0..tty5`, `ttyS0`; **рестарт при выходе** |
| **Супервизор зомби** | цикл `wait4(-1, NULL, WNOHANG)` — обязателен |
| Уровни | `0` halt, `1` single, `3` multi-user console, `6` reboot; `telinit <n>` |
| Авария | если `init` умирает → `panic("Attempted to kill init!")` (как в Linux) |

### 12.2 Службы

| Служба | Файл | Логи | pid |
|---|---|---|---|
| `syslogd` | `/etc/init.d/syslogd` | `/var/log/messages`, `/var/log/auth.log`, `/var/log/wtmp` | `/run/syslogd.pid` |
| `crond` | `/etc/init.d/crond` | `/var/log/cron` | `/run/crond.pid` |
| `networking` | `/etc/init.d/networking` | `/var/log/messages` | — |
| `sshd` | `/etc/init.d/sshd` | `/var/log/auth.log` | `/run/sshd.pid` |

**Интерфейс службы (SysV):** `start | stop | restart | status | reload`, возврат `0` — успех, `1` — ошибка, `3` — не запущена. Скрипты — на `/bin/sh`.

**Сетевой стек → службы:** `networking` поднимает `lo`, затем `eth0` (DHCP или static), затем стартует `sshd`.

---

## 13. Управление пакетами

```
pkg install <name>          # загрузить, проверить подпись, распаковать
pkg remove <name>
pkg update                  # обновить индекс репозитория
pkg upgrade-system          # обновить всё
pkg list / info <name> / verify
```

| Аспект | Требование |
|---|---|
| База | `/var/lib/pkg/db` — список установленных файлов, версии, зависимости |
| Кэш | `/var/cache/pkg/*.aurpkg` |
| Формат | `.aurpkg` = `tar` + `zstd/gzip` + `MANIFEST` (имя, версия, зависимости, checksum) |
| Подпись | detached `SHA-256` + **Ed25519/RSA-2048**, ключи в `/etc/pkg/trusted/` |
| Целостность | проверка **до** распаковки; при несоответствии — отказ и удаление |
| Зависимости | разрешение в порядке топологической сортировки |
| Атомарность | распаковка во временный каталог → `rename()`; при сбое — откат |
| Скрипты | `pre-install`, `post-install`, `pre-remove`, `post-remove` |

---

## 14. Безопасность

### 14.1 Учётные данные

`/etc/passwd` (мир, `0644`):
```
root:x:0:0:root:/root:/bin/sh
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
aurora:x:1000:1000:Aurora User:/home/aurora:/bin/sh
```

`/etc/shadow` (`0640`, root:shadow):
```
root:$6$rounds=656000$<salt>$<hash>:19000:0:99999:7:::
aurora:$6$...:19000:0:99999:7:::
```
— **SHA-512 crypt**, соль ≥ 8 символов, `rounds` настраивается, `nologin` блокирует вход.

`/etc/group`:
```
root:x:0:root
wheel:x:10:root
tty:x:5:
disk:x:6:
users:x:100:aurora
```

### 14.2 Механизмы

| Механизм | Требование |
|---|---|
| Права | `rwx` × owner/group/other + `setuid`, `setgid`, `sticky` |
| `umask` | `022` для пользователей, `077` для root-сессий |
| `setuid` | `execve` с `S_ISUID` → `euid = owner`; **`nosuid` на ФС обязателен к уважению** |
| `su` | пароль → `setreuid`; root → любой пользователь без пароля; лог в `/var/log/auth.log` |
| `login`/`getty` | чтение shadow, ограничение попыток, таймаут, вывод `motd` |
| Stack canaries | **в libc и всех системных утилитах**: случайный канареечный паттерн в TLS (`fs:`), проверка при возврате, `__stack_chk_fail` → `SIGABRT` |
| NX | `NX`-бит в PTE; стек и куча **не-exec**; `mmap` без `PROT_EXEC` по умолчанию для данных |
| Проверки | `copy_from_user`/`copy_to_user` — единственные пути обмена; `EFAULT` при плохом указателе |
| Разделение | ring 0 / ring 3, TSS, `swapgs`, проверка `CS/SS` при возврате |
| Аудит | `/var/log/auth.log`: вход, `su`, `sudo`-подобные операции |

---

## 15. Дерево исходного кода

```
aurora/
├── Makefile                     # корневой: all, kernel, libc, user, image, clean, run
├── README.md
├── SPECIFICATION.md              # этот документ
├── LICENSE
│
├── boot/                        # [ ] загрузчик
│   ├── boot.S                   #   MBR / multiboot2 header, GDT, переход в long mode
│   ├── gdt.S  idt_asm.S  switch.S
│   └── grub.cfg
│
├── kernel/                      # [ ] ядро
│   ├── main.c                   #   kmain(mbi2*)
│   ├── panic.c  printk.c  kassert.c
│   ├── arch/x86_64/
│   │   ├── cpu.c  gdt.c  idt.c  tss.c  pic.c  pit.c  apic.c
│   │   ├── syscall_entry.S  context.S  interrupts.c  exceptions.c
│   │   └── longmode.c  msr.c  cpuid.c
│   ├── mm/
│   │   ├── pmm.c  vmm.c  paging.c  heap.c  slab.c  vm_area.c  fault.c
│   ├── proc/
│   │   ├── task.c  fork.c  exec.c  exit.c  wait.c  sched.c
│   │   ├── signal.c  elf.c  thread.c
│   ├── syscall/
│   │   ├── syscall.c  syscall.tbl  # генерирует syscall.h + таблицу
│   │   ├── sys_file.c  sys_proc.c  sys_mm.c  sys_net.c  sys_misc.c
│   ├── fs/
│   │   ├── vfs.c  dcache.c  inode.c  file.c  mount.c  namei.c  path.c
│   │   ├── ramfs.c  ext4.c  ext4_balloc.c  ext4_inode.c  ext4_dir.c
│   │   ├── procfs.c  sysfs.c  devtmpfs.c  tmpfs.c  pipefs.c  sockfs.c
│   │   └── cpio.c                # распаковка initrd
│   ├── dev/
│   │   ├── chardev.c  blkdev.c  devfs_register.c  mknod.c
│   │   └── tty/  tty.c  tty_line.c  vt.c  pty.c  termios.c  kbd.c
│   ├── net/
│   │   ├── netif.c  eth.c  arp.c  ipv4.c  icmp.c  udp.c  tcp.c
│   │   ├── socket.c  sock_file.c  dhcp.c  dns.c  checksum.c
│   ├── drivers/
│   │   ├── serial.c  vga.c  rtc.c  pci.c  acpi.c
│   │   ├── ata.c  ahci.c  e1000.c  virtio.c  virtio_net.c
│   └── lib/
│       ├── string.c  printf.c  spinlock.c  waitqueue.c  list.c  radix.c
│
├── libc/                        # [ ] системная библиотека
│   ├── syscalls/                #   обёртки: .S для каждого вызова
│   ├── stdio/  stdlib/  string/  unistd/  signal/  time/
│   ├── net/                     #   socket, inet_pton, getaddrinfo
│   ├── termios/  pwd/  dirent/
│   └── crt0.S                   #   _start
│
├── user/                        # [ ] приложения
│   ├── sh/                      #   /bin/sh
│   ├── coreutils/               #   ls cat cp mv rm mkdir rmdir ln touch ...
│   ├── text/                    #   vi grep sed awk man
│   ├── proc/                    #   ps kill top uptime
│   ├── admin/                   #   mount umount fsck mkfs fdisk mknod su login getty passwd
│   ├── net/                     #   ifconfig ping wget ssh sshd netstat nslookup
│   └── pkg/                     #   pkg
│
├── include/
│   ├── aurora/                  #   ядерные заголовки
│   └── sys/  arpa/  netinet/    #   пользовательские (устанавливаются в /usr/include)
│
├── rootfs/                      # [ ] эталонный корень (копируется в образ)
│   ├── etc/  (passwd shadow group fstab rc.conf init.d/ network/ ssh/)
│   ├── bin/ sbin/ lib/ usr/ var/ home/ root/
│   └── dev/                     #   только каталоги; узлы создаёт devtmpfs
│
├── man/                         # [ ] man1/ man5/ man8/
├── scripts/
│   ├── build-toolchain.sh       #   кросс-компилятор binutils+gcc
│   ├── mkimage.sh               #   dd + parted + mkfs.ext4 + grub-install
│   ├── mkinitrd.sh              #   cpio → initrd.cpio
│   └── run-qemu.sh
├── docs/                        # ARCH, ABI, PORTING, CODING_STYLE
└── tests/
    ├── unit/                    #   pmm, vmm, vfs, ext4, tcp state machine
    ├── integration/             #   boot → login → ps → ping → pkg
    └── posix/                   #   выборка из POSIX test suite
```

---

## 16. Совместимость

| Стандарт | Уровень | Что означает в AuroraOS |
|---|---|---|
| **POSIX.1-2017** | Полная | `fork/exec/wait`, сигналы, `termios`, `unistd`-набор, `errno`, права |
| **SUSv4** | Полная | `sh` с job control, `vi`, `man`, утилиты |
| **FHS 3.0** | Полная | дерево из 7.6, symlink-совместимости |
| **Linux x86-64 syscall ABI** | Полная (нумерация) | `unistd_64.h`-совместимые номера, возврат `-errno` |
| **ELF x86-64** | Полная | `EM_X86_64`, `PT_LOAD`, auxv |
| **Multiboot 2 / 1** | Полная | GRUB-совместимость |
| **MBR / BIOS** | Полная | legacy-загрузка |
| **RFC 791 (IPv4)** | Полная | |
| **RFC 792 (ICMP)** | Полная | |
| **RFC 768 (UDP)** | Полная | |
| **RFC 9293 (TCP)** | Полная | полная конечная машина |
| **RFC 826 (ARP)** | Полная | |
| **RFC 2131 (DHCP)** | Полная | клиент |
| **RFC 1035 (DNS)** | Полная | резолвер |
| **LSB** | Частичная | `/etc/os-release`, `init.d`-интерфейс |
| **iBCS / Linux-бинарники** | ❌ | не запускаем чужие ELF (только собранные под AuroraOS) |

---

## 17. Инструменты и сборка

### 17.1 Кросс-инструментарий (обязателен)

| Компонент | Версия/требование |
|---|---|
| Целевая тройка | `x86_64-aurora-elf` |
| binutils | `ld`, `objcopy`, `readelf`, `nm` |
| gcc | `-ffreestanding -fno-stack-protector` (ядро) / `-fstack-protector-strong` (user) |
| Стили | ядро: `-Wall -Wextra -Werror -O2 -fno-omit-frame-pointer`; user: `-O2 -fstack-protector-strong` |
| Хост | Linux/WSL2 только для **сборки**; работа — в QEMU |

### 17.2 Цели Makefile

```
make toolchain     # сборка кросс-компилятора (однократно)
make kernel        # boot/ + kernel/ → build/aurora.elf
make libc          # libc/ → build/lib/libc.a, libc.so
make user          # user/ → build/rootfs/bin, sbin, usr/bin
make man           # man/ → build/rootfs/usr/share/man
make initrd        # cpio из build/rootfs → build/initrd.cpio
make image         # aurora.img: MBR + /boot( ext2 ) + / ( ext4 ) + grub
make run           # qemu-system-x86_64 с serial на stdio
make debug         # qemu -s -S + gdb
make test          # tests/unit + tests/integration
make docs          # сборка документации
make clean
```

### 17.3 Запуск

```bash
qemu-system-x86_64 \
  -machine q35 -m 512M -smp 1 \
  -hda build/aurora.img \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -serial stdio -no-reboot -no-shutdown
```

Отладка:
```bash
qemu-system-x86_64 ... -s -S &
gdb -q build/aurora.elf -ex 'target remote :1234' -ex 'continue'
```

---

## 18. Дорожная карта v1.0 (всё — с нуля)

> Порядок **обязателен**: каждая веха проверяема до начала следующей. Ни одна веха не считается закрытой без работающего теста.

| Веха | Содержание | Критерий приёмки | Статус |
|---|---|---|---|
| **M0** | Каркас репозитория, toolchain, Makefile, `panic`, `printk`, serial | В QEMU в serial: `AuroraOS 1.0.0 booting...` | `[x]` |
| **M1** | MBR/Multiboot2, GDT, переход в long mode, paging, IDT, исключения | Двойное_fault не возникает; `#PF` печатает адрес | `[x]` |
| **M2** | PMM (bitmap), VMM, direct map, heap `kmalloc/kfree` | Тест: 1 млн alloc/free без утечек | `[x]` |
| **M3** | PIT 100 Гц, `task_struct`, context switch, планировщик, `sched_yield` | Два цикла печатают `A`/`B` вперемешку | `[x]` |
| **M4** | Syscall gate (`syscall`/`int 0x80`), `write`, `exit`, `getpid`, `brk` | Пользовательский ring-3 код печатает в консоль и выходит | `[ ]` |
| **M5** | `fork`, `wait4`, `execve` (ELF64), стек `argv/envp/auxv`, сигналы | Программа делает `fork` → `execve("/bin/hello")` → `wait4` → код возврата | `[ ]` |
| **M6** | VFS: `inode/dentry/file`, namei, `open/read/write/close/lseek/stat` | Файлы в `ramfs` читаются/пишутся | `[ ]` |
| **M7** | `ramfs` + initrd + `cpio`; `procfs`; `devtmpfs`; `tmpfs`; `sysfs` | `/proc/meminfo`, `/dev/console`, `/tmp` работают | `[ ]` |
| **M8** | **`ext2/ext4` R/W**, `mkfs`, `fsck`, монтирование корня, `pivot_root`, `/etc/fstab` | Загрузка с диска, корень — ext4, файлы переживают перезагрузку | `[ ]` |
| **M9** | TTY: line discipline, VT0–5, kbd, `termios`, canonical/raw, `Ctrl-C` | `Ctrl-C` убивает foreground-процесс; `vi` работает в raw | `[ ]` |
| **M10** | libc полная + `crt0` + сборка user-утилит | `ls`, `cat`, `cp`, `ps`, `vi`, `grep` собраны и работают | `[ ]` |
| **M11** | `/bin/sh` с конвейерами, редиректами, job control; `init` (PID 1) + `getty` + `login` | **Полный сценарий: power-on → `aurora login:` → `root` → shell** | `[ ]` |
| **M12** | Блочные устройства: `pci`, `ata/ahci`, `/dev/sda*`, `mknod`, `major/minor` | `fdisk -l`, `mount /dev/sda2 /mnt` работают | `[ ]` |
| **M13** | Сеть: `eth` → `arp` → `ipv4` → `icmp`/`udp`/`tcp`, sockets, `lo`, `e1000`, `virtio-net` | `ping 10.0.2.2` получает ответы; TCP-соединение устанавливается | `[ ]` |
| **M14** | DHCP, DNS, `ifconfig`, `netstat`, `wget`, `pty` | `wget http://...` качает файл через DHCP-адрес | `[ ]` |
| **M15** | Безопасность: `passwd/shadow/group`, `su`, `login`, `umask`, `setuid`, NX, stack canaries | Вход по паролю; `su aurora` меняет UID; canary ловит переполнение | `[ ]` |
| **M16** | Службы: `syslogd`, `crond`, `networking`, `sshd`, `init.d`, уровни | `sshd` принимает подключение с хоста; `crond` запускает задачу | `[ ]` |
| **M17** | `pkg` + подпись + `/var/lib/pkg`, `man`-страницы для всех команд | `pkg install X` ставит и `man X` работает | `[ ]` |
| **M18** | Тесты (unit/integration/posix), документация, `dmesg`, стабильность 24 ч | Прогон `make test` — 100 % зелёных; аптайм 24 ч без паники | `[ ]` |

**Оценка объёма:** ~60–80 тыс. строк C + ~5 тыс. строк ассемблера + ~10 тыс. строк скриптов/документации.

---

## 19. Критерии приёмки v1.0 (Definition of Done)

Система считается **полностью рабочей**, только если выполняются **все** пункты:

### 19.1 Загрузка
- [ ] Включение QEMU → без вмешательства появляется `aurora login:` на `tty0` и на `ttyS0`.
- [ ] Холодный старт занимает < 5 секунд.
- [ ] Перезагрузка (`reboot`) работает корректно, ФС не повреждается.

### 19.2 Процессы
- [ ] `fork` + `execve` + `wait4` — полный цикл, коды возврата и сигналы корректны.
- [ ] `ps aux` показывает `init`, службы, shell, детей; поля `PID TTY STAT TIME COMMAND`.
- [ ] Зомби не накапливаются: после 10 000 циклов `fork/exit` количество процессов стабильно.
- [ ] `Ctrl-C`, `Ctrl-Z`, `fg`, `bg` работают в конвейерах.

### 19.3 Файлы и устройства
- [ ] `ls -l /dev` показывает **символьные и блочные** узлы с `major,minor` в колонке размера — как в Linux.
- [ ] `mknod /dev/test c 1 3 && cat /dev/test` → пустой ввод, exit 0.
- [ ] `dd if=/dev/zero of=/mnt/f bs=1M count=64 && md5sum` — данные совпадают после перезагрузки.
- [ ] `mount`, `umount`, `df`, `stat -f` работают; `/etc/fstab` соблюдается.
- [ ] `/proc/<pid>/status`, `/proc/meminfo`, `/proc/cpuinfo`, `/sys/class/net/eth0/` содержательны.
- [ ] `fsck` на повреждённой ФС находит и чинит ошибки.

### 19.4 Сеть
- [ ] `ifconfig eth0` поднят по DHCP; `ping -c 3 10.0.2.2` — 0 % потерь.
- [ ] TCP: установка, передача 100 МБ без ошибок, корректный `FIN`/`TIME_WAIT`.
- [ ] `wget` скачивает файл и он побайтно совпадает с оригиналом.
- [ ] `ssh` с хоста: аутентификация по паролю, **pty выделен**, `Ctrl-C` работает в удалённой сессии.
- [ ] `netstat -an` показывает сокеты в правильных состояниях.

### 19.5 Оболочка и утилиты
- [ ] Все утилиты из 11.4 присутствуют, имеют `man`-страницу, возвращают корректные коды.
- [ ] `sh -c 'cat /etc/passwd | grep root | wc -l'` → `1`.
- [ ] `vi` полностью функционален в raw-режиме: создание, редактирование, сохранение.
- [ ] Скрипт `/etc/init.d/` исполняется оболочкой без ошибок.

### 19.6 Безопасность
- [ ] Вход без пароля невозможен; 3 неудачные попытки → задержка.
- [ ] `/etc/shadow` имеет режим `0640` и недоступен обычному пользователю.
- [ ] `su aurora` → `id` показывает `uid=1000`; обратный `su` требует пароль root.
- [ ] Переполнение буфера в тестовой программе → `SIGABRT` (canary), а не выполнение кода.
- [ ] `mmap(PROT_WRITE|PROT_EXEC)` вне явного запроса невозможен; NX активен.

### 19.7 Пакеты и службы
- [ ] `pkg install` / `remove` / `upgrade-system` — атомарно, с проверкой подписи.
- [ ] Подделанный пакет отклоняется с `EACCES`/ошибкой подписи.
- [ ] `syslogd` пишет в `/var/log/messages`; `crond` выполняет задачу по расписанию; всё стартует автоматически.

### 19.8 Стабильность
- [ ] Непрерывная работа **24 часа** под нагрузкой (fork-шторм + I/O + сеть) без паники и утечек памяти.
- [ ] Любая пользовательская ошибка → `-errno`, **никогда** — `panic`.
- [ ] `make test` — 100 % прохождений; POSIX-выборка — без регрессий.

---

## 20. Тестирование

| Уровень | Что проверяется | Инструмент |
|---|---|---|
| **Unit (в ядре)** | PMM (аллок/фри/утечки), VMM (CoW, fault), slab, checksum, TCP-машина состояний, ext4 balloc | встроенный `kunit`-фреймворк, вывод в serial |
| **Unit (user)** | libc: `string`, `printf`, `malloc`, `inet_pton` | `tests/unit/user/` |
| **Integration** | полный сценарий загрузки → логин → `ps` → `ping` → `pkg` → `reboot` | QEMU + скрипт `tests/integration/run.sh`, таймауты, авто-оценка |
| **POSIX** | выборка POSIX test suite: `fork/exec/wait`, сигналы, `termios`, права | `tests/posix/` |
| **Stress** | 100 000 `fork`, 10 ГБ I/O, 1000 одновременных TCP | `tests/stress/` |
| **Regression** | образ v1.0 прогоняется после каждого коммита | CI: `make image && make test` |

**Правило:** код без теста не принимается. Новая подсистема → тест в том же коммите.

---

## 21. Ограничения, риски, компромиссы

| Решение | Компромисс | Обоснование | Риск и смягчение |
|---|---|---|---|
| Монолитное ядро | меньше изоляции | проще, быстрее, традиционнее для Unix | сбой драйвера роняет систему → `panic()` с full dump в serial |
| UP в v1.0 (SMP → v1.1) | нет распараллеливания | сложность блокировок срывает сроки | структуры `spinlock`/`per-cpu` заложены с M3 |
| `ext4` без полного `jbd2` | нет восстановления после сбоя | journal — огромный объём работы | `data=writeback` + `fsck` при каждом монтировании с `pass≠0` |
| Только ELF64 | нет 32-битных программ | один ABI = меньше багов | `compat`-слой в v1.1 |
| Статическая линковка приоритетна | крупные бинарники | нет `ld.so` в initrd | динамический `ld-aurora.so` в v1.1 |
| SysV init, без `systemd` | меньше функциональности | прозрачность, POSIX-совместимость | `init.d` — стандарт, переносим |
| Нет namespaces/cgroups | нет контейнеров | вне объёма v1.0 | архитектура VFS/proc не препятствует |
| `int 0x80` оставлен | медленнее `syscall` | отладка и совместимость | основной путь — `syscall` |

**Главные технические риски:**

1. **TCP** — самая часто недоделываемая часть. Митигация: конечная машина покрыта unit-тестами до первого пакета по_wire_.
2. **ext4** — совместимость с реальным `mkfs.ext4`. Митигация: тестировать против образа, созданного хостовым `mkfs`.
3. **PTY + ssh** — без корректного `termios` ssh непригоден. Митигация: M9 до M16, тест «ssh + vi + Ctrl-C».
4. **Объём** — 18 вех. Митигация: жёсткий запрет начинать M(n+1) без закрытой M(n).

---

## 22. Версионирование и процесс изменений

| Правило | Формула |
|---|---|
| Версия | `MAJOR.MINOR.PATCH` — семантически |
| `MAJOR` | ломка syscall ABI / формата ФС |
| `MINOR` | новые подсистемы, обратно совместимо |
| `PATCH` | исправления |
| Стабильность ABI | нумерация syscall **не меняется никогда**; новые — только в конец таблицы |
| Изменение спецификации | только через PR с обоснованием + одобрение техкомитета; раздел 21 обязан быть обновлён |
| Код | `docs/CODING_STYLE.md`: K&R-стиль ядра, 8-символьные табы, `snake_case`, префиксы подсистем (`vfs_`, `tcp_`, `pmm_`) |

---

## 23. Глоссарий

| Термин | Значение |
|---|---|
| **VFS** | виртуальная ФС — единый интерфейс ко всем ФС |
| **inode** | объект метаданных файла (номер, права, владелец, размер, время, `rdev`) |
| **dentry** | элемент кэша путей «имя → inode» |
| **major:minor** | номер устройства: класс драйвера + экземпляр |
| **devtmpfs** | ФС, в которой **ядро само** создаёт `/dev/*` при регистрации драйвера |
| **fd** | файловый дескриптор — целочисленная ручка на открытый объект |
| **TTY** | терминал: линия дисциплины + ввод/вывод + сигналы + pgid |
| **pty** | псевдотерминал: пара master/slave, нужен для `ssh` |
| **CoW** | copy-on-write — копирование страницы при первой записи |
| **init** | PID 1, предок всех процессов, супервизор зомби |
| **runlevel** | уровень работы: 0 halt, 1 single, 3 console, 6 reboot |
| **ELF** | формат исполняемых файлов |
| **auxv** | вспомогательный вектор, который ядро кладёт на стек новому процессу |
| **errno** | код ошибки последнего неудачного вызова |

---

## Приложение A. Эталонная сессия (критерий «полностью рабочая»)

```text
AuroraOS 1.0.0 (x86_64) — tty0
Kernel 1.0.0 #1 SMP  PCI  ACPI  e1000

aurora login: root
Password:
Last login: Mon Jan  1 00:00:12 on tty0

root@aurora:~# uname -a
AuroraOS aurora 1.0.0 #1 SMP x86_64 AuroraOS libc

root@aurora:~# cat /etc/os-release
NAME="AuroraOS"
ID=aurora
VERSION="1.0"
PRETTY_NAME="AuroraOS 1.0 (console)"

root@aurora:~# ls -l /dev | head
total 0
crw-rw-rw- 1 root root     1, 3 Jan  1 00:00 null
crw-rw-rw- 1 root root     1, 5 Jan  1 00:00 zero
crw-rw-rw- 1 root root     1, 9 Jan  1 00:00 urandom
crw--w---- 1 root tty      4, 0 Jan  1 00:00 tty0
crw--w---- 1 root tty      4,64 Jan  1 00:00 ttyS0
crw-rw-rw- 1 root tty      5, 0 Jan  1 00:00 tty
crw------- 1 root root     5, 1 Jan  1 00:00 console
brw-rw---- 1 root disk     8, 0 Jan  1 00:00 sda
brw-rw---- 1 root disk     8, 1 Jan  1 00:00 sda1
brw-rw---- 1 root disk     8, 2 Jan  1 00:00 sda2

root@aurora:~# mount
/dev/sda2 on / type ext4 (rw,relatime)
/dev/sda1 on /boot type ext2 (rw,nodev,nosuid)
proc on /proc type proc (rw)
sysfs on /sys type sysfs (rw)
devtmpfs on /dev type devtmpfs (rw)
tmpfs on /run type tmpfs (rw,size=16M)
tmpfs on /tmp type tmpfs (rw,nosuid,nodev)

root@aurora:~# ps aux
USER   PID %CPU  TTY STAT  TIME COMMAND
root     1  0.0  ?   Ss    0:00 /sbin/init
root   112  0.0  ?   S     0:00 /usr/sbin/syslogd
root   118  0.0  ?   S     0:00 /usr/sbin/crond
root   130  0.0  ?   S     0:00 /usr/sbin/sshd
root   141  0.0 tty0 S     0:00 /sbin/getty tty0
root   143  0.0 tty0 S     0:00 /bin/login
root   144  0.0 tty0 S     0:00 -/bin/sh

root@aurora:~# cat /proc/meminfo | head -3
MemTotal:         514232 kB
MemFree:          471884 kB
MemAvailable:     468120 kB

root@aurora:~# ifconfig eth0
eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
      inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.255
      ether 52:54:00:12:34:56  txqueuelen 1000

root@aurora:~# ping -c 3 10.0.2.2
PING 10.0.2.2 (10.0.2.2): 56 data bytes
64 bytes from 10.0.2.2: seq=0 ttl=255 time=0.42 ms
64 bytes from 10.0.2.2: seq=1 ttl=255 time=0.38 ms
64 bytes from 10.0.2.2: seq=2 ttl=255 time=0.41 ms
--- 10.0.2.2 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss

root@aurora:~# cat /etc/passwd | grep root | wc -l
1

root@aurora:~# ls -l /etc/shadow
-rw-r----- 1 root shadow 412 Jan  1 00:00 /etc/shadow

root@aurora:~# pkg install coreutils-extra
[1/3] verifying signature ....... OK (ed25519)
[2/3] unpacking coreutils-extra-1.0.0.aurpkg
[3/3] registering in /var/lib/pkg/db
installed coreutils-extra-1.0.0

root@aurora:~# man ls
LS(1)                     AuroraOS User Manual                    LS(1)
NAME
       ls - list directory contents
...

root@aurora:~# seq 1 5 | grep -v 3 | wc -l
4

root@aurora:~# vi /etc/motd          # full-screen, raw mode, save & quit

root@aurora:~# su aurora
aurora@aurora:~$ id
uid=1000(aurora) gid=1000(users) groups=1000(users)
aurora@aurora:~$ ls -l /etc/shadow
ls: cannot open '/etc/shadow': Permission denied

aurora@aurora:~$ exit
root@aurora:~# shutdown -r now
Broadcast message from root@aurora:
The system is going down for reboot NOW!
AuroraOS: syncing filesystems... unmounting /tmp /run / ...
reboot: System halted
```

---

## Приложение B. Чек-лист готовности v1.0 (полный, всё — `[ ]`)

```
[x] M0  Каркас, toolchain, printk, serial
[x] M1  Multiboot2, GDT, long mode, IDT, исключения
[x] M2  PMM, VMM, paging, kmalloc/kfree
[x] M3  PIT, task_struct, context switch, планировщик
[ ] M4  syscall gate, write/exit/getpid/brk
[ ] M5  fork, wait4, execve (ELF64), сигналы
[ ] M6  VFS: inode/dentry/file, namei, open/read/write
[ ] M7  ramfs+initrd, procfs, devtmpfs, tmpfs, sysfs
[ ] M8  ext2/ext4 R/W, mkfs, fsck, pivot_root, fstab
[ ] M9  TTY, VT0-5, kbd, termios, Ctrl-C
[ ] M10 libc + crt0 + сборка утилит
[ ] M11 /bin/sh + init + getty + login  →  ЛОГИН РАБОТАЕТ
[ ] M12 pci, ata/ahci, /dev/sda*, mknod, major:minor
[ ] M13 eth/arp/ipv4/icmp/udp/tcp + sockets + e1000/virtio
[ ] M14 DHCP, DNS, ifconfig, netstat, wget, pty
[ ] M15 passwd/shadow/group, su, login, umask, setuid, NX, canaries
[ ] M16 syslogd, crond, networking, sshd, init.d, runlevels
[ ] M17 pkg + подписи + man-страницы
[ ] M18 тесты, документация, 24h стабильность
```

---

> **Статус документа:** финальная спецификация **AuroraOS 1.0**.
> **Состояние реализации:** 🟥 **0 % — проект начинается с нуля.**
> Любые отклонения от настоящего документа допускаются только с оформленным решением в разделе 21 и одобрения технического комитета.