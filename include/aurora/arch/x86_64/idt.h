#ifndef _AURORA_ARCH_X86_64_IDT_H
#define _AURORA_ARCH_X86_64_IDT_H

#include <aurora/types.h>

#define IDT_ENTRIES 256

#define IDT_GATE_INTERRUPT  0x8E /* Present, Ring 0, 64-bit Interrupt Gate */
#define IDT_GATE_USER_INT   0xEE /* Present, Ring 3, 64-bit Interrupt Gate (e.g. for int 0x80) */
#define IDT_GATE_TRAP       0x8F /* Present, Ring 0, 64-bit Trap Gate */

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

typedef struct idt_entry idt_entry_t;
typedef struct idt_ptr idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, void *handler, uint8_t type_attr, uint8_t ist);

#endif /* _AURORA_ARCH_X86_64_IDT_H */
