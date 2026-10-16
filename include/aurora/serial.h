#ifndef _AURORA_SERIAL_H
#define _AURORA_SERIAL_H

#include <aurora/types.h>

#define COM1_PORT 0x3F8

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *str, size_t len);
void serial_puts(const char *str);
int serial_has_rx(void);
char serial_getc(void);

#endif /* _AURORA_SERIAL_H */
