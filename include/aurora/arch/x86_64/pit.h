#ifndef _AURORA_ARCH_X86_64_PIT_H
#define _AURORA_ARCH_X86_64_PIT_H

#include <aurora/types.h>

#define PIT_TARGET_HZ 100 /* 100 Hz = 10 ms quantum (Section 3.2 & Section 18) */

void pit_init(uint32_t frequency_hz);
uint64_t pit_get_ticks(void);

#endif /* _AURORA_ARCH_X86_64_PIT_H */
