.global _start
.extern main
.extern exit
.extern __libc_start_main

.section .text
_start:
    /* 0. Set up Thread Local Storage (%fs) for stack protector at %fs:0x28 */
    mov $158, %rax          /* SYS_arch_prctl */
    mov $0x1002, %rdi       /* ARCH_SET_FS */
    lea __tls_area(%rip), %rsi
    syscall

    /* 1. Clear frame pointer */
    xor %rbp, %rbp

    /* 2. Pop argc into %rsi */
    pop %rsi

    /* 3. %rdx = argv */
    mov %rsp, %rdx

    /* 4. %rcx = envp = argv + (argc + 1) * 8 */
    lea 8(%rdx, %rsi, 8), %rcx

    /* 5. 16-byte align stack */
    and $-16, %rsp

    /* 6. First argument to __libc_start_main is main */
    lea main(%rip), %rdi

    /* Now calling __libc_start_main(main, argc, argv, envp) */
    call __libc_start_main

    /* 7. Exit if returned */
    mov %rax, %rdi
    call exit

1:  jmp 1b

.section .data
.align 16
__tls_area:
    .fill 5, 8, 0
    .quad 0x595e9fbd94fda766   /* Offset 0x28 (40 bytes): stack protector canary */
    .fill 10, 8, 0
