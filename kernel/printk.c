#include <aurora/printk.h>
#include <aurora/serial.h>
#include <aurora/string.h>

struct fmt_stream {
    char   *buf;
    size_t  size;
    size_t  count;
    bool    to_serial;
};

#define KMSG_RING_SIZE 32768
static char g_kmsg_ring[KMSG_RING_SIZE];
static size_t g_kmsg_head = 0;
static size_t g_kmsg_total = 0;

static void stream_putc(struct fmt_stream *s, char c)
{
    if (s->to_serial) {
        serial_putchar(c);
        g_kmsg_ring[g_kmsg_head] = c;
        g_kmsg_head = (g_kmsg_head + 1) % KMSG_RING_SIZE;
        if (g_kmsg_total < KMSG_RING_SIZE) {
            g_kmsg_total++;
        }
        s->count++;
    } else {
        if (s->buf && s->size > 0 && s->count + 1 < s->size) {
            s->buf[s->count] = c;
        }
        s->count++;
    }
}

int64_t sys_syslog(int type, char *buf, int len)
{
    /* 10: SYSLOG_ACTION_SIZE_BUFFER */
    if (type == 10) {
        return (int64_t)g_kmsg_total;
    }
    /* 2: READ, 3: READ_ALL */
    if (type == 2 || type == 3) {
        if (!buf || len <= 0) return -22; /* -EINVAL */
        size_t to_copy = (g_kmsg_total < (size_t)len) ? g_kmsg_total : (size_t)len;
        size_t start = (g_kmsg_total == KMSG_RING_SIZE) ? g_kmsg_head : 0;
        for (size_t i = 0; i < to_copy; i++) {
            buf[i] = g_kmsg_ring[(start + i) % KMSG_RING_SIZE];
        }
        return (int64_t)to_copy;
    }
    return 0;
}

static void stream_puts(struct fmt_stream *s, const char *str)
{
    while (*str) {
        stream_putc(s, *str++);
    }
}

static void stream_print_num(struct fmt_stream *s, int64_t num, int base, bool is_signed, bool uppercase, int width, char pad)
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

    int total_len = i + (negative ? 1 : 0);

    if (pad == '0' && negative) {
        stream_putc(s, '-');
        negative = false;
        width--;
    }

    while (total_len < width && pad == ' ') {
        stream_putc(s, ' ');
        width--;
    }

    if (negative) {
        stream_putc(s, '-');
    }

    while (i < width && pad == '0') {
        stream_putc(s, '0');
        width--;
    }

    while (i > 0) {
        stream_putc(s, buf[--i]);
    }
}

static int format_internal(struct fmt_stream *s, const char *fmt, va_list args)
{
    /* Check for Linux-style loglevel prefix like "<6>" only when outputting to serial */
    if (s->to_serial && fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>') {
        switch (fmt[1]) {
            case '0': stream_puts(s, "[EMERG] "); break;
            case '1': stream_puts(s, "[ALERT] "); break;
            case '2': stream_puts(s, "[CRIT]  "); break;
            case '3': stream_puts(s, "[ERR]   "); break;
            case '4': stream_puts(s, "[WARN]  "); break;
            case '5': stream_puts(s, "[NOTE]  "); break;
            case '6': stream_puts(s, "[INFO]  "); break;
            case '7': stream_puts(s, "[DEBUG] "); break;
        }
        fmt += 3;
    }

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            stream_putc(s, *p);
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
                stream_putc(s, c);
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                if (!str) {
                    str = "(null)";
                }
                while (*str) {
                    stream_putc(s, *str++);
                }
                break;
            }
            case 'd':
            case 'i': {
                int64_t val = is_long ? va_arg(args, int64_t) : va_arg(args, int);
                stream_print_num(s, val, 10, true, false, width, pad);
                break;
            }
            case 'u': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                stream_print_num(s, val, 10, false, false, width, pad);
                break;
            }
            case 'x': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                stream_print_num(s, val, 16, false, false, width, pad);
                break;
            }
            case 'X': {
                uint64_t val = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                stream_print_num(s, val, 16, false, true, width, pad);
                break;
            }
            case 'p': {
                uint64_t val = (uint64_t)va_arg(args, void *);
                stream_puts(s, "0x");
                stream_print_num(s, val, 16, false, false, 16, '0');
                break;
            }
            case '%': {
                stream_putc(s, '%');
                break;
            }
            default:
                stream_putc(s, '%');
                stream_putc(s, *p);
                break;
        }
    }

    if (!s->to_serial && s->buf && s->size > 0) {
        if (s->count < s->size) {
            s->buf[s->count] = '\0';
        } else {
            s->buf[s->size - 1] = '\0';
        }
    }

    return (int)s->count;
}

int vprintk(const char *fmt, va_list args)
{
    struct fmt_stream s = {
        .buf = NULL,
        .size = 0,
        .count = 0,
        .to_serial = true,
    };
    return format_internal(&s, fmt, args);
}

int printk(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vprintk(fmt, args);
    va_end(args);
    return ret;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    struct fmt_stream s = {
        .buf = buf,
        .size = size,
        .count = 0,
        .to_serial = false,
    };
    return format_internal(&s, fmt, args);
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}
