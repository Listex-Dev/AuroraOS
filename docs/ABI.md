# Спецификация двоичного интерфейса приложений (ABI) AuroraOS

**Версия:** 1.0.0  
**Целевая архитектура:** x86-64 (AMD64 / Intel 64)  
**Совместимость:** Соглашение о системных вызовах и ABI Linux x86-64

---

## 1. Механизм системных вызовов

AuroraOS поддерживает оба стандартных механизма входа в ядро для x86-64:
1. **Инструкция `syscall` (Быстрый путь, основной)**:
   - Использует модельно-специфичные регистры MSR: `IA32_STAR`, `IA32_LSTAR` (`syscall_entry`) и `IA32_FMASK` (маскирует флаги `IF`, `DF`, `TF`).
   - Изменяет регистры `RCX` (сохраняет адрес возврата `RIP` пользователя) и `R11` (сохраняет регистр флагов `RFLAGS`).
2. **Прерывание `int 0x80` (Устаревший шлюз)**:
   - Вектор 128 в таблице IDT с уровнем привилегий DPL=3 для совместимости со старыми ассемблерными программами.

### Соглашение о передаче параметров через регистры

| Параметр | Регистр пользователя | Аргумент функции ядра |
|---|---|---|
| **Номер системного вызова** | `RAX` | Индекс в таблице диспетчеризации |
| **Аргумент 1** | `RDI` | `a1` |
| **Аргумент 2** | `RSI` | `a2` |
| **Аргумент 3** | `RDX` | `a3` |
| **Аргумент 4** | `R10` *(внимание: `RCX` занят инструкцией `syscall`)* | `a4` |
| **Аргумент 5** | `R8` | `a5` |
| **Аргумент 6** | `R9` | `a6` |
| **Возвращаемое значение** | `RAX` | `>= 0` при успехе; `-errno` при ошибке |

**Сохраняемые регистры (Callee-Preserved):**  
`RBX`, `RBP`, `RSP`, `R12`, `R13`, `R14`, `R15`. Ядро гарантирует полное сохранение значений этих регистров при возврате в пространство пользователя.

---

## 2. Разметка памяти процесса и инициализация стека

### Карта виртуального адресного пространства

```
0x00007FFFFFFFFFFF ──> Верхняя граница доступного адресного пространства пользователя
0x00007FFFFFFFE000 ──> Начальный указатель стека (RSP), растёт ВНИЗ (бит NX активен)
                         │
                         ▼
0x00000000XXXXXXXX ──> Граница динамической кучи (brk), растёт ВВЕРХ через sys_brk
0x000000000042XXXX ──> Сегменты BSS и инициализированных данных (чтение/запись, NX)
0x0000000000400000 ──> Сегмент исполняемого кода (.text, чтение/исполнение)
0x0000000000000000 ──> Защитная нулевая страница (не отображена, ловит разыменование NULL)
```

### Структура стека при запуске (`execve`)

При передаче управления на точку входа программы `_start` регистр `RSP` указывает на следующую структуру данных:

```
[ Старшие адреса ]
   Область строк: строки аргументов argv, строки переменных envp, путь к бинарнику
   -----------------------------------------------------------------
   Маркер завершения вспомогательного вектора: AT_NULL (0, 0)
   Элемент вектора 2: { AT_PAGESZ, 4096 }
   Элемент вектора 1: { AT_ENTRY, адрес_точки_входа }
   Элемент вектора 0: { AT_PHDR, адрес_заголовков_программы }
   NULL (завершает массив envp)
   envp[M-1] -> указатель на "PATH=/bin:/usr/bin"
   ...
   envp[0]   -> указатель на "USER=root"
   NULL (завершает массив argv)
   argv[N-1] -> указатель на строку аргумента N-1
   ...
   argv[0]   -> указатель на имя программы
   argc      -> 64-битное целое число количества аргументов
[ RSP ] ──────────────────────────────────────────────────────────
[ Младшие адреса ]
```

---

## 3. Таблица системных вызовов

Все номера системных вызовов полностью совпадают со стандартной нумерацией Linux x86-64:

| Номер | Имя вызова | Сигнатура на C | Описание назначения |
|---|---|---|---|
| `0` | `read` | `ssize_t read(int fd, void *buf, size_t count)` | Чтение данных из файлового дескриптора |
| `1` | `write` | `ssize_t write(int fd, const void *buf, size_t count)` | Запись данных в файловый дескриптор |
| `2` | `open` | `int open(const char *filename, int flags, mode_t mode)` | Открытие или создание файла |
| `3` | `close` | `int close(int fd)` | Закрытие открытого файлового дескриптора |
| `4` | `stat` | `int stat(const char *filename, struct stat *statbuf)` | Получение атрибутов файла по пути |
| `5` | `fstat` | `int fstat(int fd, struct stat *statbuf)` | Получение атрибутов файла по дескриптору |
| `8` | `lseek` | `off_t lseek(int fd, off_t offset, int whence)` | Перемещение позиции чтения/записи в файле |
| `9` | `mmap` | `void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)` | Отображение страниц в адресное пространство |
| `10` | `mprotect` | `int mprotect(void *addr, size_t len, int prot)` | Изменение прав доступа к страницам памяти |
| `11` | `munmap` | `int munmap(void *addr, size_t len)` | Освобождение отображённых страниц памяти |
| `12` | `brk` | `void *brk(void *addr)` | Изменение границы сегмента данных (кучи) |
| `13` | `rt_sigaction` | `int rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact, size_t sigsetsize)` | Установка и чтение обработчика сигнала |
| `14` | `rt_sigprocmask` | `int rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize)` | Изменение маски заблокированных сигналов |
| `15` | `rt_sigreturn` | `int rt_sigreturn(void)` | Возврат из пользовательского батута сигнала |
| `16` | `ioctl` | `int ioctl(int fd, unsigned long req, ...)` | Управление параметрами устройства (termios) |
| `21` | `access` | `int access(const char *filename, int mode)` | Проверка прав доступа пользователя к файлу |
| `22` | `pipe` | `int pipe(int pipefd[2])` | Создание однонаправленного канала связи |
| `32` | `dup` | `int dup(int oldfd)` | Дублирование файлового дескриптора |
| `33` | `dup2` | `int dup2(int oldfd, int newfd)` | Дублирование дескриптора в конкретный номер |
| `35` | `nanosleep` | `int nanosleep(const struct timespec *req, struct timespec *rem)` | Высокоточная приостановка потока |
| `39` | `getpid` | `pid_t getpid(void)` | Получение идентификатора текущего процесса |
| `41` | `socket` | `int socket(int domain, int type, int protocol)` | Создание точки сетевого подключения (сокета) |
| `42` | `connect` | `int connect(int fd, const struct sockaddr *addr, socklen_t len)` | Установление соединения с удалённым хостом |
| `43` | `accept` | `int accept(int fd, struct sockaddr *addr, socklen_t *len)` | Принятие входящего соединения на сокете |
| `44` | `sendto` | `ssize_t sendto(int fd, const void *buf, size_t len, int flags, ...)` | Отправка сообщения в сокет |
| `45` | `recvfrom` | `ssize_t recvfrom(int fd, void *buf, size_t len, int flags, ...)` | Приём сообщения из сокета |
| `46` | `sendmsg` | `ssize_t sendmsg(int fd, const struct msghdr *msg, int flags)` | Отправка составного сообщения в сокет |
| `47` | `recvmsg` | `ssize_t recvmsg(int fd, struct msghdr *msg, int flags)` | Приём составного сообщения из сокета |
| `48` | `shutdown` | `int shutdown(int fd, int how)` | Закрытие части двустороннего соединения |
| `49` | `bind` | `int bind(int fd, const struct sockaddr *addr, socklen_t len)` | Привязка сокета к локальному адресу и порту |
| `50` | `listen` | `int listen(int fd, int backlog)` | Перевод сокета в режим ожидания соединений |
| `51` | `getsockname` | `int getsockname(int fd, struct sockaddr *addr, socklen_t *len)` | Получение локального адреса сокета |
| `52` | `getpeername` | `int getpeername(int fd, struct sockaddr *addr, socklen_t *len)` | Получение адреса удалённого узла |
| `53` | `socketpair` | `int socketpair(int d, int type, int protocol, int sv[2])` | Создание пары связанных сокетов |
| `54` | `setsockopt` | `int setsockopt(int fd, int level, int optname, const void *val, socklen_t len)` | Установка параметров сокета |
| `55` | `getsockopt` | `int getsockopt(int fd, int level, int optname, void *val, socklen_t *len)` | Чтение параметров сокета |
| `56` | `clone` | `int clone(unsigned long flags, void *child_stack, ...)` | Создание дочернего потока или процесса |
| `57` | `fork` | `pid_t fork(void)` | Создание дочернего процесса-копии |
| `59` | `execve` | `int execve(const char *filename, char *const argv[], char *const envp[])` | Загрузка и исполнение новой программы |
| `60` | `exit` | `void exit(int error_code)` | Завершение работы текущего процесса |
| `61` | `wait4` | `pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru)` | Ожидание смены состояния дочернего процесса |
| `62` | `kill` | `int kill(pid_t pid, int sig)` | Отправка сигнала указанному процессу |
| `63` | `uname` | `int uname(struct utsname *buf)` | Получение информации об операционной системе |
| `83` | `mkdir` | `int mkdir(const char *pathname, mode_t mode)` | Создание нового каталога |
| `84` | `rmdir` | `int rmdir(const char *pathname)` | Удаление пустого каталога |
| `87` | `unlink` | `int unlink(const char *pathname)` | Удаление записи файла из файловой системы |
| `90` | `chmod` | `int chmod(const char *filename, mode_t mode)` | Изменение прав доступа к файлу |
| `92` | `chown` | `int chown(const char *filename, uid_t user, gid_t group)` | Изменение владельца и группы файла |
| `95` | `umask` | `mode_t umask(mode_t mask)` | Установка системной маски создания файлов |
| `102` | `getuid` | `uid_t getuid(void)` | Получение реального ID пользователя |
| `103` | `syslog` | `int syslog(int type, char *buf, int len)` | Чтение и управление журналом ядра (`klogctl`) |
| `104` | `getgid` | `gid_t getgid(void)` | Получение реального ID группы |
| `105` | `setuid` | `int setuid(uid_t uid)` | Установка реального и эффективного UID |
| `106` | `setgid` | `int setgid(gid_t gid)` | Установка реального и эффективного GID |
| `107` | `geteuid` | `uid_t geteuid(void)` | Получение эффективного ID пользователя |
| `108` | `getegid` | `gid_t getegid(void)` | Получение эффективного ID группы |
| `165` | `mount` | `int mount(const char *dev, const char *dir, const char *type, unsigned long flags, const void *data)` | Монтирование файловой системы |
| `166` | `umount2` | `int umount2(const char *target, int flags)` | Размонтирование файловой системы |
| `169` | `reboot` | `int reboot(int magic1, int magic2, int cmd, void *arg)` | Перезагрузка или остановка компьютера |
| `217` | `getdents64` | `int getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count)` | Чтение записей каталога |
| `293` | `pipe2` | `int pipe2(int pipefd[2], int flags)` | Создание канала с флагами `O_CLOEXEC`/`O_NONBLOCK` |

---

## 4. Обработка сигналов и сигнальный кадр

При возникновении сигнала, для которого зарегистрирован пользовательский обработчик:
1. Ядро формирует на пользовательском стеке сигнальный кадр, включающий структуру контекста регистров (`struct ucontext`), структуру `siginfo_t` и батут возврата, указывающий на `sys_rt_sigreturn` (системный вызов 15).
2. Обработчик сигнала выполняется на уровне Ring 3.
3. По завершении обработчика управление переходит на батут, вызывающий `sys_rt_sigreturn`, который атомарно восстанавливает контекст регистров, сохранённый до прерывания сигналом.
