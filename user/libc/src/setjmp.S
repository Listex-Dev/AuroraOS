.global setjmp
.global longjmp

.section .text

setjmp:
    mov %rbx, 0(%rdi)
    mov %rsp, 8(%rdi)
    mov %rbp, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %r13, 32(%rdi)
    mov %r14, 40(%rdi)
    mov %r15, 48(%rdi)
    mov (%rsp), %rdx
    mov %rdx, 56(%rdi)
    xor %rax, %rax
    ret

longjmp:
    mov %rsi, %rax
    test %rax, %rax
    jnz 1f
    mov $1, %rax
1:
    mov 0(%rdi), %rbx
    mov 8(%rdi), %rsp
    mov 16(%rdi), %rbp
    mov 24(%rdi), %r12
    mov 32(%rdi), %r13
    mov 40(%rdi), %r14
    mov 48(%rdi), %r15
    mov 56(%rdi), %rdx
    mov %rdx, (%rsp)
    ret
