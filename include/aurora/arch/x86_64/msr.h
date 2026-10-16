#ifndef _AURORA_ARCH_X86_64_MSR_H
#define _AURORA_ARCH_X86_64_MSR_H

#include <aurora/types.h>

#define MSR_EFER             0xC0000080
#define MSR_STAR             0xC0000081
#define MSR_LSTAR            0xC0000082
#define MSR_CSTAR            0xC0000083
#define MSR_SFMASK           0xC0000084
#define MSR_FS_BASE          0xC0000100
#define MSR_GS_BASE          0xC0000101
#define MSR_KERNEL_GS_BASE   0xC0000102

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

#endif /* _AURORA_ARCH_X86_64_MSR_H */
