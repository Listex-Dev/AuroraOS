#!/bin/bash
set -e

mkdir -p build/libc build/bin

CFLAGS="-Wall -Wextra -Werror -O2 -fno-builtin -nostdlib -fno-pie -no-pie -fstack-protector-strong -Iuser/libc/include"

echo "=== Building AuroraOS libc ==="
gcc $CFLAGS -c user/libc/src/crt0.S -o build/libc/crt0.o
gcc $CFLAGS -c user/libc/src/setjmp.S -o build/libc/setjmp.o

for src in errno string malloc stdio stdlib unistd dirent termios time signal pwd socket pty dns crypt syslog; do
    echo "  CC libc: $src.c"
    gcc $CFLAGS -c user/libc/src/${src}.c -o build/libc/${src}.o
done

echo "  AR build/libc/libc.a"
ar rcs build/libc/libc.a build/libc/*.o

echo "=== Building AuroraOS User Utilities ==="
for bin in ls cat cp ps vi grep sleep test_m10 sh getty login init test_m11 mknod fdisk mount test_m12 ping test_m13 dhclient nslookup ifconfig netstat wget test_m14 id su chmod test_m15 syslogd logger crond crontab sshd telinit touch echo test_m16; do
    echo "  CC/LD user bin: $bin"
    gcc $CFLAGS -static user/bin/${bin}.c build/libc/crt0.o build/libc/libc.a -o build/bin/${bin}
done

echo "=== Userland Build Succeeded ==="
ls -lh build/bin/
