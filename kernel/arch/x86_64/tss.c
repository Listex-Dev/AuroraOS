#include <aurora/arch/x86_64/tss.h>
#include <aurora/string.h>

static tss_entry_t kernel_tss;

/* Dedicated stack for Double Fault (#DF) handler to guarantee no Triple Fault */
static uint8_t double_fault_stack[4096] __attribute__((aligned(16)));

tss_entry_t *g_kernel_tss_ptr = &kernel_tss;

void tss_init(void)
{
    memset(&kernel_tss, 0, sizeof(kernel_tss));

    /* IST1: Dedicated stack for Double Faults */
    kernel_tss.ist1 = (uint64_t)(double_fault_stack + sizeof(double_fault_stack));

    /* I/O Map Base set past TSS limit (disables I/O port bitmap) */
    kernel_tss.iomap_base = sizeof(kernel_tss);
}

void tss_set_rsp0(uint64_t rsp0)
{
    kernel_tss.rsp0 = rsp0;
}

void tss_set_ist(int index, uint64_t stack_top)
{
    switch (index) {
        case 1: kernel_tss.ist1 = stack_top; break;
        case 2: kernel_tss.ist2 = stack_top; break;
        case 3: kernel_tss.ist3 = stack_top; break;
        case 4: kernel_tss.ist4 = stack_top; break;
        case 5: kernel_tss.ist5 = stack_top; break;
        case 6: kernel_tss.ist6 = stack_top; break;
        case 7: kernel_tss.ist7 = stack_top; break;
        default: break;
    }
}
