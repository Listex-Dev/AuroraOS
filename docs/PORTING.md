# Портирование приложений в AuroraOS

В этом руководстве описываются процессы портирования стороннего программного обеспечения на C/C++, компиляции пользовательских бинарных файлов и создания криптографически подписанных пакетов для **AuroraOS 1.0**.

---

## 1. Инструменты сборки и целевое окружение

AuroraOS использует стандартную модель данных LP64 для архитектуры x86-64:
- `sizeof(int) = 4`
- `sizeof(long) = 8`
- `sizeof(void *) = 8`
- `sizeof(size_t) = 8`

Исполняемые файлы в AuroraOS 1.0 представляют собой статически слинкованные бинарные файлы формата ELF64 с точкой входа по адресу `0x0000000000401000`.

### Необходимые флаги компилятора

При кросс-компиляции на хост-системе (Linux / WSL) используются следующие параметры:

```bash
GCC_FLAGS="-nostdinc -isystem $(PWD)/user/libc/include \
           -fno-pie -no-pie -fno-builtin \
           -fstack-protector-strong \
           -Wall -Wextra -O2"

LD_FLAGS="-static -nostdlib"
```

---

## 2. Сборка с системной библиотекой AuroraOS Libc

AuroraOS предоставляет реализацию стандартной библиотеки Си в каталоге `user/libc/`.

### Процесс линковки

Сборка и линковка пользовательской программы `myprog.c`:

```bash
# 1. Компиляция исходного кода в объектный файл
x86_64-linux-gnu-gcc $GCC_FLAGS -c myprog.c -o myprog.o

# 2. Статическая линковка с crt0.o и libc.a
x86_64-linux-gnu-ld $LD_FLAGS \
    build/libc/crt0.o \
    myprog.o \
    build/libc/libc.a \
    -o myprog
```

### Поддерживаемые стандартные заголовочные файлы

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

## 3. Управление пакетами и дистрибуция (`.aurpkg`)

Все пакеты программного обеспечения для AuroraOS упаковываются в защищённый формат архивов `.aurpkg`, снабжённых криптографической цифровой подписью HMAC-SHA256.

### Формат бинарного пакета

```
+-----------------------------------------------------------+
| Сигнатура (Magic): "AURPKG01" (8 байт)                    |
+-----------------------------------------------------------+
| Имя пакета (32 байта, дополнено нулями)                   |
+-----------------------------------------------------------+
| Версия пакета (16 байт, например "1.0.0\0")               |
+-----------------------------------------------------------+
| Флаги пакета (uint32_t)                                   |
+-----------------------------------------------------------+
| Общее количество файлов (uint32_t)                        |
+-----------------------------------------------------------+
| Цифровая подпись HMAC-SHA256 (64 байта hex-строки)        |
+-----------------------------------------------------------+
| Заголовок файла 0:                                        |
| - Целевой путь установки: например "/usr/bin/myprog" (128)|
| - Права доступа (File Mode): например 0755 (uint32_t)     |
| - Размер файла: в байтах (uint32_t)                       |
| - Содержимое файла: сырые байты данных                    |
+-----------------------------------------------------------+
| ... Последующие файлы 1..N ...                            |
+-----------------------------------------------------------+
```

### Создание пакетов на хосте

Для сборки пакетов используется утилита `tools/pkg-build`:

1. Подготовьте каталог со структурой файлов для установки:
   ```bash
   mkdir -p staging/usr/bin staging/usr/share/man/man1
   cp myprog staging/usr/bin/
   cp myprog.1 staging/usr/share/man/man1/
   ```

2. Соберите подписанный архив `.aurpkg` с использованием доверенного ключа релиза:
   ```bash
   ./tools/pkg-build create \
       --name myprog \
       --version 1.0.0 \
       --key user/etc/pkg/trusted/aurora.key \
       --dir staging \
       --output build/packages/myprog-1.0.0.aurpkg
   ```

3. Проверка пакета на стороне хоста:
   ```bash
   ./tools/pkg-build verify \
       --package build/packages/myprog-1.0.0.aurpkg \
       --key user/etc/pkg/trusted/aurora.key
   ```

### Управление пакетами в AuroraOS

Запустите AuroraOS в QEMU и управляйте пакетами утилитой `/bin/pkg`:

```bash
# Установка пакета (проверка подписи и атомарная распаковка)
pkg install /packages/myprog-1.0.0.aurpkg

# Список установленных пакетов
pkg list

# Просмотр детальной информации о пакете
pkg info myprog

# Просмотр установленного руководства
man myprog

# Удаление пакета
pkg remove myprog
```
