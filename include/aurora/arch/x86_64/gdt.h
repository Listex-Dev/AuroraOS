#ifndef _AURORA_ARCH_X86_64_GDT_H
#define _AURORA_ARCH_X86_64_GDT_H

#include <aurora/types.h>

#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_DATA_SEL   0x1B /* 0x18 | RPL 3 */
#define GDT_USER_CODE_SEL   0x23 /* 0x20 | RPL 3 */
#define GDT_TSS_SEL         0x28

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

typedef struct gdt_entry gdt_entry_t;
typedef struct gdt_ptr gdt_ptr_t;

void gdt_init(void);

#endif /* _AURORA_ARCH_X86_64_GDT_H */
