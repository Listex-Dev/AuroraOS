#ifndef _AURORA_PRINTK_H
#define _AURORA_PRINTK_H

#include <aurora/types.h>

/* Linux-compatible loglevel prefixes */
#define KERN_EMERG   "<0>" /* System is unusable */
#define KERN_ALERT   "<1>" /* Action must be taken immediately */
#define KERN_CRIT    "<2>" /* Critical conditions */
#define KERN_ERR     "<3>" /* Error conditions */
#define KERN_WARNING "<4>" /* Warning conditions */
#define KERN_NOTICE  "<5>" /* Normal but significant condition */
#define KERN_INFO    "<6>" /* Informational */
#define KERN_DEBUG   "<7>" /* Debug-level messages */

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int snprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

#endif /* _AURORA_PRINTK_H */
