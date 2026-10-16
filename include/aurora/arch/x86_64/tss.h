#ifndef _AURORA_ARCH_X86_64_TSS_H
#define _AURORA_ARCH_X86_64_TSS_H

#include <aurora/types.h>

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry tss_entry_t;

extern tss_entry_t kernel_tss;

void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);
void tss_set_ist(int index, uint64_t stack_top);

#endif /* _AURORA_ARCH_X86_64_TSS_H */
