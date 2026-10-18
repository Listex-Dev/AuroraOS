#!/bin/bash
set -e

mkdir -p build/libc build/bin

CFLAGS="-Wall -Wextra -Werror -O2 -fno-builtin -nostdlib -fno-pie -no-pie -Iuser/libc/include"

echo "=== Building AuroraOS libc ==="
gcc $CFLAGS -c user/libc/src/crt0.S -o build/libc/crt0.o
gcc $CFLAGS -c user/libc/src/setjmp.S -o build/libc/setjmp.o

for src in errno string malloc stdio stdlib unistd dirent termios time signal pwd; do
    echo "  CC libc: $src.c"
    gcc $CFLAGS -c user/libc/src/${src}.c -o build/libc/${src}.o
done

echo "  AR build/libc/libc.a"
ar rcs build/libc/libc.a build/libc/*.o

echo "=== Building AuroraOS User Utilities ==="
for bin in ls cat cp ps vi grep sleep test_m10 sh getty login init test_m11; do
    echo "  CC/LD user bin: $bin"
    gcc $CFLAGS -static user/bin/${bin}.c build/libc/crt0.o build/libc/libc.a -o build/bin/${bin}
done

echo "=== Userland Build Succeeded ==="
ls -lh build/bin/
