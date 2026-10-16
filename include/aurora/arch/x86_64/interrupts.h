#ifndef _AURORA_ARCH_X86_64_INTERRUPTS_H
#define _AURORA_ARCH_X86_64_INTERRUPTS_H

#include <aurora/types.h>

struct interrupt_frame {
    /* Pushed by common assembly stub (in reverse order of push) */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    /* Pushed by vector stub */
    uint64_t int_no;
    uint64_t error_code;

    /* Pushed automatically by CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

typedef struct interrupt_frame interrupt_frame_t;

typedef void (*isr_handler_t)(interrupt_frame_t *frame);

void exceptions_init(void);
void isr_register_handler(uint8_t vector, isr_handler_t handler);
void isr_unregister_handler(uint8_t vector);

/* C entry point called from idt_asm.S */
void exception_dispatch(interrupt_frame_t *frame);

/* Diagnostic helper to print registers */
void dump_registers(const interrupt_frame_t *frame);

#endif /* _AURORA_ARCH_X86_64_INTERRUPTS_H */
