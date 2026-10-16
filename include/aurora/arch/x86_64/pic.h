#ifndef _AURORA_ARCH_X86_64_PIC_H
#define _AURORA_ARCH_X86_64_PIC_H

#include <aurora/types.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

/* IRQ base vectors in IDT */
#define PIC_MASTER_OFFSET 32
#define PIC_SLAVE_OFFSET  40

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif /* _AURORA_ARCH_X86_64_PIC_H */
