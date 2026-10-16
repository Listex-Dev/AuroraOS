#include <aurora/arch/x86_64/gdt.h>
#include <aurora/arch/x86_64/tss.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern tss_entry_t *g_kernel_tss_ptr;

/*
 * GDT Layout:
 * 0: Null descriptor
 * 1: Kernel Code 64 (0x08)
 * 2: Kernel Data 64 (0x10)
 * 3: User Data 64   (0x18 | 3 = 0x1B)
 * 4: User Code 64   (0x20 | 3 = 0x23)
 * 5-6: TSS (16 bytes, 0x28)
 */
static struct {
    gdt_entry_t entries[5];
    struct tss_descriptor tss;
} __attribute__((packed)) g_gdt;

static gdt_ptr_t g_gdt_ptr;

static void set_gdt_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    g_gdt.entries[idx].base_low = (base & 0xFFFF);
    g_gdt.entries[idx].base_middle = (base >> 16) & 0xFF;
    g_gdt.entries[idx].base_high = (base >> 24) & 0xFF;

    g_gdt.entries[idx].limit_low = (limit & 0xFFFF);
    g_gdt.entries[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    g_gdt.entries[idx].access = access;
}

static void set_tss_descriptor(uint64_t base, uint32_t limit)
{
    g_gdt.tss.limit_low = (limit & 0xFFFF);
    g_gdt.tss.base_low = (base & 0xFFFF);
    g_gdt.tss.base_middle = (base >> 16) & 0xFF;
    g_gdt.tss.access = 0x89; /* Present, DPL 0, 64-bit TSS (Available) */
    g_gdt.tss.granularity = ((limit >> 16) & 0x0F);
    g_gdt.tss.base_high = (base >> 24) & 0xFF;
    g_gdt.tss.base_upper = (base >> 32) & 0xFFFFFFFF;
    g_gdt.tss.reserved = 0;
}

void gdt_init(void)
{
    memset(&g_gdt, 0, sizeof(g_gdt));

    /* 0: Null descriptor */
    set_gdt_entry(0, 0, 0, 0, 0);

    /* 1: Kernel Code 64-bit (0x08): Executable, Readable, DPL 0, Long mode */
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0x20);

    /* 2: Kernel Data 64-bit (0x10): Writable, DPL 0 */
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0x00);

    /* 3: User Data 64-bit (0x18): Writable, DPL 3 */
    set_gdt_entry(3, 0, 0xFFFFF, 0xF2, 0x00);

    /* 4: User Code 64-bit (0x20): Executable, Readable, DPL 3, Long mode */
    set_gdt_entry(4, 0, 0xFFFFF, 0xFA, 0x20);

    /* Initialize TSS */
    tss_init();

    /* 5-6: TSS Descriptor (0x28) */
    set_tss_descriptor((uint64_t)g_kernel_tss_ptr, sizeof(tss_entry_t) - 1);

    g_gdt_ptr.limit = sizeof(g_gdt) - 1;
    g_gdt_ptr.base = (uint64_t)&g_gdt;

    /* Load new GDT, reload CS with 0x08, and load Task Register */
    __asm__ volatile (
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "mov $0x28, %%ax\n\t"
        "ltr %%ax\n\t"
        : : "r"(&g_gdt_ptr) : "rax", "memory"
    );

    printk("<6>[  0.000000] GDT: 64-bit descriptors configured (Kernel CS=0x08, DS=0x10; User CS=0x23, DS=0x1B)\n");
    printk("<6>[  0.000000] TSS: Initialized and loaded into TR (Selector 0x28, IST1 active for #DF)\n");
}
