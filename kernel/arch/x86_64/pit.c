#include <aurora/arch/x86_64/pit.h>
#include <aurora/arch/x86_64/pic.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/io.h>
#include <aurora/printk.h>

#define PIT_BASE_FREQ 1193182
#define PIT_CHANNEL_0 0x40
#define PIT_COMMAND   0x43

static volatile uint64_t g_pit_ticks = 0;

extern void sched_timer_tick(void);

static void pit_irq_handler(interrupt_frame_t *frame)
{
    (void)frame;
    g_pit_ticks++;

    /* Acknowledge interrupt to PIC */
    pic_send_eoi(0);

    /* Notify scheduler */
    sched_timer_tick();
}

void pit_init(uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        frequency_hz = PIT_TARGET_HZ;
    }

    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;
    if (divisor > 65535) {
        divisor = 65535;
    }

    /* Command: Channel 0, lobyte/hibyte, Mode 3 (Square Wave), Binary 16-bit */
    outb(PIT_COMMAND, 0x36);
    io_wait();

    outb(PIT_CHANNEL_0, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL_0, (uint8_t)((divisor >> 8) & 0xFF));
    io_wait();

    /* Register IRQ0 (Vector 32) handler */
    isr_register_handler(PIC_MASTER_OFFSET + 0, pit_irq_handler);

    /* Unmask IRQ0 on PIC */
    pic_clear_mask(0);

    printk("<6>[  0.000000] PIT: Timer active at %u Hz (quantum 10 ms, divisor %u)\n",
           (unsigned int)frequency_hz, (unsigned int)divisor);
}

uint64_t pit_get_ticks(void)
{
    return g_pit_ticks;
}
