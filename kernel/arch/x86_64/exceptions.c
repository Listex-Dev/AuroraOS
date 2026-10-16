#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/printk.h>
#include <aurora/panic.h>
#include <aurora/string.h>

static const char *exception_messages[32] = {
    "#DE: Divide-by-zero Error",
    "#DB: Debug",
    "#NMI: Non-maskable Interrupt",
    "#BP: Breakpoint",
    "#OF: Overflow",
    "#BR: Bound Range Exceeded",
    "#UD: Invalid Opcode",
    "#NM: Device Not Available",
    "#DF: Double Fault",
    "Coprocessor Segment Overrun",
    "#TS: Invalid TSS",
    "#NP: Segment Not Present",
    "#SS: Stack-Segment Fault",
    "#GP: General Protection Fault",
    "#PF: Page Fault",
    "Reserved",
    "#MF: x87 Floating-Point Exception",
    "#AC: Alignment Check",
    "#MC: Machine Check",
    "#XM: SIMD Floating-Point Exception",
    "#VE: Virtualization Exception",
    "#CP: Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#HV: Hypervisor Injection Exception",
    "#VC: VMM Communication Exception",
    "#SX: Security Exception",
    "Reserved"
};

static isr_handler_t g_isr_handlers[256];

void isr_register_handler(uint8_t vector, isr_handler_t handler)
{
    g_isr_handlers[vector] = handler;
}

void isr_unregister_handler(uint8_t vector)
{
    g_isr_handlers[vector] = NULL;
}

void dump_registers(const interrupt_frame_t *frame)
{
    printk("-------------------------------------------------------\n");
    printk("RIP: %p  CS: 0x%x  RFLAGS: %p\n",
           (void *)frame->rip, (unsigned int)frame->cs, (void *)frame->rflags);
    printk("RSP: %p  SS: 0x%x  Error Code: 0x%x\n",
           (void *)frame->rsp, (unsigned int)frame->ss, (unsigned int)frame->error_code);
    printk("RAX: %p  RBX: %p  RCX: %p\n",
           (void *)frame->rax, (void *)frame->rbx, (void *)frame->rcx);
    printk("RDX: %p  RSI: %p  RDI: %p\n",
           (void *)frame->rdx, (void *)frame->rsi, (void *)frame->rdi);
    printk("RBP: %p  R8:  %p  R9:  %p\n",
           (void *)frame->rbp, (void *)frame->r8, (void *)frame->r9);
    printk("R10: %p  R11: %p  R12: %p\n",
           (void *)frame->r10, (void *)frame->r11, (void *)frame->r12);
    printk("R13: %p  R14: %p  R15: %p\n",
           (void *)frame->r13, (void *)frame->r14, (void *)frame->r15);
    printk("CR0: %p  CR2: %p  CR3: %p  CR4: %p\n",
           (void *)read_cr0(), (void *)read_cr2(), (void *)read_cr3(), (void *)read_cr4());
    printk("-------------------------------------------------------\n");
}

static void handle_page_fault(interrupt_frame_t *frame)
{
    uint64_t cr2 = read_cr2();
    uint64_t err = frame->error_code;

    bool present = (err & (1 << 0)) != 0;
    bool write   = (err & (1 << 1)) != 0;
    bool user    = (err & (1 << 2)) != 0;
    bool rsvd    = (err & (1 << 3)) != 0;
    bool ifetch  = (err & (1 << 4)) != 0;

    printk("<0>[EXCEPTION] Page Fault (#PF)!\n");
    printk("<0>Faulting linear address (CR2): 0x%p\n", (void *)cr2);
    printk("<0>Details: [%s, %s-access, %s-mode%s%s]\n",
           present ? "protection-violation" : "page-not-present",
           write ? "write" : "read",
           user ? "user" : "kernel",
           rsvd ? ", reserved-bit-violation" : "",
           ifetch ? ", instruction-fetch" : "");

    dump_registers(frame);
}

void exception_dispatch(interrupt_frame_t *frame)
{
    uint64_t vec = frame->int_no;

    /* Check if a registered custom handler handles this vector */
    if (g_isr_handlers[vec] != NULL) {
        g_isr_handlers[vec](frame);
        return;
    }

    if (vec == 14) {
        handle_page_fault(frame);
        panic("Unhandled Kernel Page Fault at 0x%p", (void *)read_cr2());
    } else if (vec == 8) {
        printk("<0>[EXCEPTION] Double Fault (#DF) on IST1 stack!\n");
        dump_registers(frame);
        panic("Double Fault (#DF)");
    } else if (vec < 32) {
        printk("<0>[EXCEPTION] %s (Vector %d)\n", exception_messages[vec], (int)vec);
        dump_registers(frame);
        panic("Kernel CPU Exception: %s", exception_messages[vec]);
    } else {
        printk("<4>[IRQ] Unhandled Interrupt Vector %d\n", (int)vec);
    }
}

void exceptions_init(void)
{
    memset(g_isr_handlers, 0, sizeof(g_isr_handlers));
    printk("<6>[  0.000000] Exceptions: handlers registered for vectors 0-31\n");
}
