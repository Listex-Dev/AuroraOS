#include <aurora/arch/x86_64/idt.h>
#include <aurora/arch/x86_64/gdt.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern void *isr_stub_table[IDT_ENTRIES];

static idt_entry_t g_idt[IDT_ENTRIES] __attribute__((aligned(16)));
static idt_ptr_t g_idtr;

void idt_set_gate(uint8_t vector, void *handler, uint8_t type_attr, uint8_t ist)
{
    uint64_t addr = (uint64_t)handler;

    g_idt[vector].offset_low  = (uint16_t)(addr & 0xFFFF);
    g_idt[vector].selector    = GDT_KERNEL_CODE_SEL;
    g_idt[vector].ist         = (ist & 0x07);
    g_idt[vector].type_attr   = type_attr;
    g_idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    g_idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    g_idt[vector].zero        = 0;
}

void idt_init(void)
{
    memset(&g_idt, 0, sizeof(g_idt));

    /* Initialize all 256 vectors with their corresponding stubs */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        uint8_t ist = 0;
        uint8_t flags = IDT_GATE_INTERRUPT;

        /* Double Fault (#DF, vector 8) uses dedicated IST1 stack */
        if (i == 8) {
            ist = 1;
        }

        /* Syscall gate (int 0x80) allows Ring 3 user access */
        if (i == 0x80) {
            flags = IDT_GATE_USER_INT;
        }

        idt_set_gate((uint8_t)i, isr_stub_table[i], flags, ist);
    }

    g_idtr.limit = sizeof(g_idt) - 1;
    g_idtr.base = (uint64_t)&g_idt;

    __asm__ volatile ("lidt (%0)" : : "r"(&g_idtr));

    printk("<6>[  0.000000] IDT: 256 gates initialized (Vector 8 on IST1, Int 0x80 enabled for Ring 3)\n");
}
