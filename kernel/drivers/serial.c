#include <aurora/serial.h>
#include <aurora/io.h>

#define SERIAL_DATA_PORT(base)          ((base) + 0)
#define SERIAL_FIFO_COMMAND_PORT(base)  ((base) + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  ((base) + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) ((base) + 4)
#define SERIAL_LINE_STATUS_PORT(base)   ((base) + 5)

static int serial_is_transmit_empty(uint16_t base)
{
    /* Bit 5 of LSR is the Transmit Holding Register Empty (THRE) indicator */
    return inb(SERIAL_LINE_STATUS_PORT(base)) & 0x20;
}

void serial_init(void)
{
    uint16_t base = COM1_PORT;

    /* Disable all interrupts */
    outb(base + 1, 0x00);

    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_LINE_COMMAND_PORT(base), 0x80);

    /* Set divisor to 1 (115200 baud) */
    outb(SERIAL_DATA_PORT(base), 0x01); /* Low byte */
    outb(base + 1, 0x00);               /* High byte */

    /* 8 bits, no parity, 1 stop bit (8N1) */
    outb(SERIAL_LINE_COMMAND_PORT(base), 0x03);

    /* Enable FIFO, clear TX/RX queues, set 14-byte threshold */
    outb(SERIAL_FIFO_COMMAND_PORT(base), 0xC7);

    /* RTS/DSR set, Auxiliary Output 2 (enables interrupts if needed) */
    outb(SERIAL_MODEM_COMMAND_PORT(base), 0x0B);
}

void serial_putchar(char c)
{
    uint16_t base = COM1_PORT;

    if (c == '\n') {
        while (!serial_is_transmit_empty(base))
            ;
        outb(SERIAL_DATA_PORT(base), '\r');
    }

    while (!serial_is_transmit_empty(base))
        ;
    outb(SERIAL_DATA_PORT(base), (uint8_t)c);
}

void serial_write(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        serial_putchar(str[i]);
    }
}

void serial_puts(const char *str)
{
    while (*str) {
        serial_putchar(*str++);
    }
}
