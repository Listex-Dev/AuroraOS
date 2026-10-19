# Porting Applications to AuroraOS

This guide covers porting third-party C/C++ software, compiling userland binaries, and creating cryptographically signed packages for **AuroraOS 1.0**.

---

## 1. Toolchain & Target Environment

AuroraOS uses a standard LP64 data model for x86-64:
- `sizeof(int) = 4`
- `sizeof(long) = 8`
- `sizeof(void *) = 8`
- `sizeof(size_t) = 8`

Binaries in AuroraOS 1.0 are statically linked ELF64 executables with entry point at `0x0000000000401000`.

### Required Compiler Flags

When building binaries on the host system (Linux / WSL) for AuroraOS:

```bash
GCC_FLAGS="-nostdinc -isystem $(PWD)/user/libc/include \
           -fno-pie -no-pie -fno-builtin \
           -fstack-protector-strong \
           -Wall -Wextra -O2"

LD_FLAGS="-static -nostdlib"
```

---

## 2. Compiling with AuroraOS Libc

AuroraOS provides a standard POSIX C library located in `user/libc/`.

### Linking Workflow

To compile and link a program `myprog.c`:

```bash
# 1. Compile C source into object
x86_64-linux-gnu-gcc $GCC_FLAGS -c myprog.c -o myprog.o

# 2. Link against AuroraOS crt0 and libc.a
x86_64-linux-gnu-ld $LD_FLAGS \
    build/libc/crt0.o \
    myprog.o \
    build/libc/libc.a \
    -o myprog
```

### Supported C Standard Headers

- `<stdio.h>`: `printf`, `sprintf`, `snprintf`, `fopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fputs`, `stdin`, `stdout`, `stderr`.
- `<stdlib.h>`: `malloc`, `calloc`, `realloc`, `free`, `exit`, `atoi`, `strtol`, `strtoul`, `abs`.
- `<string.h>`: `strlen`, `strcpy`, `strncpy`, `strcmp`, `strncmp`, `strcat`, `strchr`, `strstr`, `memcpy`, `memmove`, `memset`, `memcmp`, `strdup`.
- `<unistd.h>`: `read`, `write`, `open`, `close`, `lseek`, `fork`, `execve`, `pipe`, `dup2`, `sleep`, `usleep`, `getpid`, `getuid`, `unlink`.
- `<fcntl.h>`: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_APPEND`, `O_TRUNC`, `O_NONBLOCK`.
- `<sys/stat.h>`: `stat`, `fstat`, `mkdir`, `chmod`, `S_ISREG`, `S_ISDIR`, `S_IRUSR`, `S_IWUSR`, `S_IXUSR`.
- `<sys/socket.h>`: `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `sendto`, `recvfrom`, `setsockopt`.
- `<netinet/in.h>`: `struct sockaddr_in`, `htons`, `ntohs`, `htonl`, `ntohl`, `INADDR_ANY`, `IPPROTO_TCP`, `IPPROTO_UDP`.
- `<arpa/inet.h>`: `inet_addr`, `inet_ntoa`, `inet_pton`, `inet_ntop`.
- `<termios.h>`: `tcgetattr`, `tcsetattr`, `cfmakeraw`, `cfsetispeed`, `cfsetospeed`.
- `<signal.h>`: `sigaction`, `signal`, `kill`, `sigprocmask`, `SIGINT`, `SIGTERM`, `SIGKILL`, `SIGSEGV`, `SIGALRM`.
- `<pwd.h>`: `getpwnam`, `getpwuid`, `struct passwd`.
- `<dirent.h>`: `opendir`, `readdir`, `closedir`, `struct dirent`.
- `<syslog.h>`: `openlog`, `syslog`, `closelog`, `LOG_INFO`, `LOG_ERR`, `LOG_DAEMON`.
- `<sha256.h>`: `sha256_hash_bytes`, `sha256_hash_file`, `sha256_hmac`.

---

## 3. Package Management & Distribution (`.aurpkg`)

All software packages for AuroraOS are packaged into `.aurpkg` archives, signed cryptographically with HMAC-SHA256.

### Package File Format

```
+-----------------------------------------------------------+
| Magic: "AURPKG01" (8 bytes)                               |
+-----------------------------------------------------------+
| Package Name (32 bytes, null-padded)                      |
+-----------------------------------------------------------+
| Package Version (16 bytes, e.g. "1.0.0\0")                |
+-----------------------------------------------------------+
| Flags (uint32_t)                                          |
+-----------------------------------------------------------+
| Total File Count (uint32_t)                               |
+-----------------------------------------------------------+
| HMAC-SHA256 Signature (64 bytes hex string)               |
+-----------------------------------------------------------+
| File Entry 0 Header:                                      |
| - Target Path: e.g. "/usr/bin/myprog" (128 bytes)         |
| - File Mode: e.g. 0755 (uint32_t)                         |
| - File Size: bytes (uint32_t)                             |
| - Data: raw bytes                                         |
+-----------------------------------------------------------+
| ... File Entry N ...                                      |
+-----------------------------------------------------------+
```

### Creating Packages

Use the `tools/pkg-build` utility on the host system:

1. Prepare a staging directory containing the files to be installed:
   ```bash
   mkdir -p staging/usr/bin staging/usr/share/man/man1
   cp myprog staging/usr/bin/
   cp myprog.1 staging/usr/share/man/man1/
   ```

2. Generate the signed `.aurpkg` archive using the release signing key:
   ```bash
   ./tools/pkg-build create \
       --name myprog \
       --version 1.0.0 \
       --key user/etc/pkg/trusted/aurora.key \
       --dir staging \
       --output build/packages/myprog-1.0.0.aurpkg
   ```

3. Verification on the host:
   ```bash
   ./tools/pkg-build verify \
       --package build/packages/myprog-1.0.0.aurpkg \
       --key user/etc/pkg/trusted/aurora.key
   ```

### Installing Packages in AuroraOS

Boot AuroraOS in QEMU and execute `/bin/pkg`:

```bash
# Install package (atomically extracts files and verifies signature)
pkg install /packages/myprog-1.0.0.aurpkg

# Query installed packages
pkg list

# Inspect package details
pkg info myprog

# View installed manual page
man myprog

# Uninstall package
pkg remove myprog
```
