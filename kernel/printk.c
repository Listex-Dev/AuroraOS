#include <aurora/printk.h>
#include <aurora/serial.h>
#include <aurora/string.h>

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

static void print_num(int64_t num, int base, bool is_signed, bool uppercase, int width, char pad)
{
    char buf[65];
    char digits_lower[] = "0123456789abcdef";
    char digits_upper[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_upper : digits_lower;

    uint64_t uval;
    bool negative = false;

    if (is_signed && num < 0) {
        negative = true;
        uval = (uint64_t)(-num);
    } else {
        uval = (uint64_t)num;
    }

    int i = 0;
    if (uval == 0) {
        buf[i++] = '0';
    } else {
        while (uval > 0) {
            buf[i++] = digits[uval % base];
            uval /= base;
        }
    }

    if (negative) {
        if (pad == '0') {
            serial_putchar('-');
            width--;
            negative = false;
        }
    }

    while (i < width && (pad == ' ' || pad == '0')) {
        serial_putchar(pad);
        width--;
    }

    if (negative) {
        serial_putchar('-');
    }

    while (i > 0) {
        serial_putchar(buf[--i]);
    }
}

int vprintk(const char *fmt, va_list args)
{
    int count = 0;

    /* Check for Linux-style loglevel prefix like "<6>" */
    if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>') {
        /* Format log level tag nicely for early console if desired */
        switch (fmt[1]) {
            case '0': serial_puts("[EMERG] "); break;
            case '1': serial_puts("[ALERT] "); break;
            case '2': serial_puts("[CRIT]  "); break;
            case '3': serial_puts("[ERR]   "); break;
            case '4': serial_puts("[WARN]  "); break;
            case '5': serial_puts("[NOTE]  "); break;
            case '6': serial_puts("[INFO]  "); break;
            case '7': serial_puts("[DEBUG] "); break;
        }
        fmt += 3;
    }

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            serial_putchar(*p);
            count++;
            continue;
        }

        p++; /* Skip '%' */
        char pad = ' ';
        int width = 0;

        if (*p == '0') {
            pad = '0';
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        bool is_long = false;
        if (*p == 'l') {
            is_long = true;
            p++;
            if (*p == 'l') {
                p++;
            }
        } else if (*p == 'z') {
            is_long = true;
            p++;
        }

        switch (*p) {
            case 'c': {
                char c = (char)va_arg(args, int);
                serial_putchar(c);
                count++;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) {
                    s = "(null)";
                }
                while (*s) {
                    serial_putchar(*s++);
                    count++;
                }
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_long ? va_arg(args, int64_t) : va_arg(args, int);
                print_num(val, 10, true, false, width, pad);
                break;
            }
            case 'u': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                print_num(val, 10, false, false, width, pad);
                break;
            }
            case 'x': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                print_num(val, 16, false, false, width, pad);
                break;
            }
            case 'X': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                print_num(val, 16, false, true, width, pad);
                break;
            }
            case 'p': {
                uint64_t val = (uint64_t)va_arg(args, void *);
                serial_puts("0x");
                print_num(val, 16, false, false, 16, '0');
                break;
            }
            case '%': {
                serial_putchar('%');
                count++;
                break;
            }
            default:
                serial_putchar('%');
                serial_putchar(*p);
                count += 2;
                break;
        }
    }

    return count;
}

int printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vprintk(fmt, args);
    va_end(args);
    return ret;
}
