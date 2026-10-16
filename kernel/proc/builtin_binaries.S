/*
 * Embed builtin user ELF64 binaries for execve
 */

.section .rodata
.global bin_hello_start
.global bin_hello_end

.align 16
bin_hello_start:
    .incbin "build/hello.elf"
bin_hello_end:
