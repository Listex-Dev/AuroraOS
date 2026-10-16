#include <aurora/panic.h>
#include <aurora/printk.h>
#include <aurora/io.h>

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap)         __builtin_va_end(ap)

extern int vprintk(const char *fmt, va_list args);

void panic(const char *fmt, ...)
{
    cli();

    printk("\n=======================================================\n");
    printk("<0>*** KERNEL PANIC ***\n");

    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);

    printk("\nSystem halted.\n");
    printk("=======================================================\n");

    while (1) {
        hlt();
    }
}
