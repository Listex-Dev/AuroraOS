#include <aurora/arch/x86_64/pic.h>
#include <aurora/io.h>
#include <aurora/printk.h>

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_init(void)
{
    /* ICW1: Start initialization sequence in cascade mode */
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    /* ICW2: Vector offset (Master -> 32, Slave -> 40) */
    outb(PIC1_DATA, PIC_MASTER_OFFSET);
    io_wait();
    outb(PIC2_DATA, PIC_SLAVE_OFFSET);
    io_wait();

    /* ICW3: Tell Master that Slave is on IRQ2 (0000 0100b), Slave identity (0000 0010b) */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086/88 mode */
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Mask all interrupts initially */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    printk("<6>[  0.000000] PIC: 8259A cascaded controllers remapped to vectors 32..47\n");
}

void pic_set_mask(uint8_t irq)
{
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq)
{
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) & ~(1 << irq);
    outb(port, value);
}
