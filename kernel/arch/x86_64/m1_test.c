#include <aurora/arch/x86_64/m1_test.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/printk.h>

#define TEST_UNMAPPED_ADDR 0x000000DEADBEEF00ULL

static volatile bool g_pf_caught = false;
static volatile uint64_t g_pf_fault_addr = 0;
static uint64_t g_pf_resume_rip = 0;

static void test_pf_handler(interrupt_frame_t *frame)
{
    g_pf_fault_addr = read_cr2();
    g_pf_caught = true;

    printk("<6>[TEST #PF] Intercepted Page Fault in IDT!\n");
    printk("<6>[TEST #PF] CR2 (Faulting Address): %p\n", (void *)g_pf_fault_addr);
    printk("<6>[TEST #PF] Error code: 0x%x (non-present, read, kernel)\n", (unsigned int)frame->error_code);
    printk("<6>[TEST #PF] Resuming execution via iretq to %p...\n", (void *)g_pf_resume_rip);

    /* Advance RIP to recovery label */
    frame->rip = g_pf_resume_rip;
}

bool test_page_fault_handling(void)
{
    printk("<6>[  0.000000] Running Milestone M1 verification test...\n");

    g_pf_caught = false;
    g_pf_fault_addr = 0;

    isr_register_handler(14, test_pf_handler);

    /* Deliberately read unmapped address TEST_UNMAPPED_ADDR */
    __asm__ volatile (
        "leaq 1f(%%rip), %%rax\n\t"
        "movq %%rax, %0\n\t"
        "movq (%1), %%rax\n\t" /* Triggers #PF */
        "1:\n\t"
        : "=m"(g_pf_resume_rip)
        : "r"(TEST_UNMAPPED_ADDR)
        : "rax", "memory"
    );

    /* Restore normal unhandled handler */
    isr_unregister_handler(14);

    if (g_pf_caught && g_pf_fault_addr == TEST_UNMAPPED_ADDR) {
        printk("<6>[  0.000000] Milestone M1 status: [OK] GDT, TSS, IDT, Exceptions active. #PF verified without Double Fault.\n");
        return true;
    } else {
        printk("<0>[  0.000000] Milestone M1 status: [FAILED] #PF was not intercepted properly!\n");
        return false;
    }
}
