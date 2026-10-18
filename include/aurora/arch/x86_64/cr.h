#ifndef _AURORA_ARCH_X86_64_CR_H
#define _AURORA_ARCH_X86_64_CR_H

#include <aurora/types.h>

static inline uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr0" : : "r"(val));
}

static inline uint64_t read_cr2(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val));
}

static inline uint64_t read_cr4(void)
{
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val)
{
    __asm__ volatile ("mov %0, %%cr4" : : "r"(val));
}

static inline void sse_init(void)
{
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1ULL << 2); /* Clear EM (coprocessor emulation) */
    cr0 |= (1ULL << 1);  /* Set MP (monitor coprocessor) */
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);  /* Set OSFXSR (FXSAVE/FXRSTOR support) */
    cr4 |= (1ULL << 10); /* Set OSXMMEXCPT (unmasked SIMD exceptions) */
    write_cr4(cr4);
}

#endif /* _AURORA_ARCH_X86_64_CR_H */
