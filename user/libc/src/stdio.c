#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static FILE _stdin_file  = { .fd = 0, .flags = O_RDONLY };
static FILE _stdout_file = { .fd = 1, .flags = O_WRONLY };
static FILE _stderr_file = { .fd = 2, .flags = O_WRONLY };

FILE *stdin  = &_stdin_file;
FILE *stdout = &_stdout_file;
FILE *stderr = &_stderr_file;

void __libc_init_stdio(void)
{
    stdin  = &_stdin_file;
    stdout = &_stdout_file;
    stderr = &_stderr_file;
}

FILE *fdopen(int fd, const char *mode)
{
    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) return NULL;
    memset(f, 0, sizeof(FILE));
    f->fd = fd;
    if (strchr(mode, '+')) f->flags = O_RDWR;
    else if (strchr(mode, 'w') || strchr(mode, 'a')) f->flags = O_WRONLY;
    else f->flags = O_RDONLY;
    return f;
}

FILE *fopen(const char *pathname, const char *mode)
{
    int flags = 0;
    if (strchr(mode, '+')) {
        flags = O_RDWR;
        if (strchr(mode, 'w')) flags |= O_CREAT | O_TRUNC;
        else if (strchr(mode, 'a')) flags |= O_CREAT | O_APPEND;
    } else if (strchr(mode, 'w')) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strchr(mode, 'a')) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        flags = O_RDONLY;
    }

    int fd = open(pathname, flags, 0666);
    if (fd < 0) return NULL;
    return fdopen(fd, mode);
}

int fclose(FILE *stream)
{
    if (!stream) return EOF;
    fflush(stream);
    int ret = close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    return ret;
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    size_t bytes_read = 0;
    char *out = (char *)ptr;

    while (stream->unget_count > 0 && bytes_read < total) {
        out[bytes_read++] = stream->unget[--stream->unget_count];
    }

    while (bytes_read < total) {
        ssize_t ret = read(stream->fd, out + bytes_read, total - bytes_read);
        if (ret < 0) {
            stream->error = 1;
            break;
        }
        if (ret == 0) {
            stream->eof = 1;
            break;
        }
        bytes_read += ret;
    }
    return bytes_read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    size_t bytes_written = 0;
    const char *in = (const char *)ptr;

    while (bytes_written < total) {
        ssize_t ret = write(stream->fd, in + bytes_written, total - bytes_written);
        if (ret <= 0) {
            stream->error = 1;
            break;
        }
        bytes_written += ret;
    }
    return bytes_written / size;
}

int fgetc(FILE *stream)
{
    if (stream->unget_count > 0) {
        return (unsigned char)stream->unget[--stream->unget_count];
    }
    unsigned char ch;
    ssize_t ret = read(stream->fd, &ch, 1);
    if (ret <= 0) {
        if (ret == 0) stream->eof = 1;
        else stream->error = 1;
        return EOF;
    }
    return ch;
}

int fputc(int c, FILE *stream)
{
    unsigned char ch = (unsigned char)c;
    ssize_t ret = write(stream->fd, &ch, 1);
    return (ret == 1) ? ch : EOF;
}

char *fgets(char *s, int size, FILE *stream)
{
    if (!s || size <= 0 || !stream) return NULL;
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputs(const char *s, FILE *stream)
{
    if (!stream) return EOF;
    if (!s) s = "(null)";
    size_t len = strlen(s);
    return (fwrite(s, 1, len, stream) == len) ? 0 : EOF;
}

int getchar(void)
{
    return fgetc(stdin);
}

int putchar(int c)
{
    return fputc(c, stdout);
}

int puts(const char *s)
{
    if (!s) s = "(null)";
    int ret = fputs(s, stdout);
    if (ret >= 0) {
        putchar('\n');
    }
    return ret;
}

int ungetc(int c, FILE *stream)
{
    if (c == EOF || !stream || stream->unget_count >= 4) return EOF;
    stream->unget[stream->unget_count++] = (char)c;
    stream->eof = 0;
    return (unsigned char)c;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (!stream) return -1;
    stream->unget_count = 0;
    return (lseek(stream->fd, offset, whence) >= 0) ? 0 : -1;
}

long ftell(FILE *stream)
{
    if (!stream) return -1;
    return lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
    if (stream) stream->error = 0;
}

int feof(FILE *stream)
{
    return stream ? stream->eof : 0;
}

int ferror(FILE *stream)
{
    return stream ? stream->error : 0;
}

void clearerr(FILE *stream)
{
    if (stream) {
        stream->eof = 0;
        stream->error = 0;
    }
}

void perror(const char *s)
{
    if (s && *s) {
        fputs(s, stderr);
        fputs(": ", stderr);
    }
    fputs(strerror(errno), stderr);
    fputc('\n', stderr);
}

/* Formatted output */
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    size_t written = 0;
    const char *p = format;

    while (*p) {
        if (*p != '%') {
            if (written + 1 < size) str[written] = *p;
            written++;
            p++;
            continue;
        }
        p++; /* skip '%' */

        int width = 0;
        int zero_pad = 0;
        int left_align = 0;
        if (*p == '-') {
            left_align = 1;
            p++;
        }
        if (*p == '0') {
            zero_pad = 1;
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') { is_long = 2; p++; }
        }

        switch (*p) {
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                size_t slen = strlen(s);
                if (!left_align) {
                    while (width > (int)slen) {
                        if (written + 1 < size) str[written] = ' ';
                        written++;
                        width--;
                    }
                }
                for (size_t i = 0; i < slen; i++) {
                    if (written + 1 < size) str[written] = s[i];
                    written++;
                }
                if (left_align) {
                    while (width > (int)slen) {
                        if (written + 1 < size) str[written] = ' ';
                        written++;
                        width--;
                    }
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                if (written + 1 < size) str[written] = c;
                written++;
                break;
            }
            case 'd':
            case 'i': {
                long long val = (is_long == 2) ? va_arg(ap, long long) :
                                (is_long == 1) ? va_arg(ap, long) : va_arg(ap, int);
                if (val < 0) {
                    if (written + 1 < size) str[written] = '-';
                    written++;
                    val = -val;
                }
                char buf[32];
                int idx = 0;
                if (val == 0) buf[idx++] = '0';
                else {
                    while (val > 0) {
                        buf[idx++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                while (width > idx) {
                    if (written + 1 < size) str[written] = zero_pad ? '0' : ' ';
                    written++;
                    width--;
                }
                while (idx > 0) {
                    if (written + 1 < size) str[written] = buf[--idx];
                    written++;
                }
                break;
            }
            case 'u': {
                unsigned long long val = (is_long == 2) ? va_arg(ap, unsigned long long) :
                                         (is_long == 1) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                char buf[32];
                int idx = 0;
                if (val == 0) buf[idx++] = '0';
                else {
                    while (val > 0) {
                        buf[idx++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                while (width > idx) {
                    if (written + 1 < size) str[written] = zero_pad ? '0' : ' ';
                    written++;
                    width--;
                }
                while (idx > 0) {
                    if (written + 1 < size) str[written] = buf[--idx];
                    written++;
                }
                break;
            }
            case 'x':
            case 'X':
            case 'p': {
                unsigned long long val = (*p == 'p') ? (uintptr_t)va_arg(ap, void *) :
                                         (is_long == 2) ? va_arg(ap, unsigned long long) :
                                         (is_long == 1) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
                if (*p == 'p') {
                    if (written + 1 < size) str[written] = '0';
                    written++;
                    if (written + 1 < size) str[written] = 'x';
                    written++;
                }
                char buf[32];
                int idx = 0;
                const char *hex = (*p == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                if (val == 0) buf[idx++] = '0';
                else {
                    while (val > 0) {
                        buf[idx++] = hex[val & 0xF];
                        val >>= 4;
                    }
                }
                while (width > idx) {
                    if (written + 1 < size) str[written] = zero_pad ? '0' : ' ';
                    written++;
                    width--;
                }
                while (idx > 0) {
                    if (written + 1 < size) str[written] = buf[--idx];
                    written++;
                }
                break;
            }
            case '%': {
                if (written + 1 < size) str[written] = '%';
                written++;
                break;
            }
            default:
                break;
        }
        p++;
    }

    if (size > 0) {
        if (written < size) str[written] = '\0';
        else str[size - 1] = '\0';
    }
    return (int)written;
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, 1048576, format, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
    char buf[4096];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    if (len > 0) {
        fwrite(buf, 1, len, stream);
    }
    return len;
}

int vprintf(const char *format, va_list ap)
{
    return vfprintf(stdout, format, ap);
}

int vsprintf(char *str, const char *format, va_list ap)
{
    return vsnprintf(str, 1048576, format, ap);
}

int printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    if (!lineptr || !n || !stream) return -1;
    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = (char *)malloc(*n);
        if (!*lineptr) return -1;
    }

    size_t pos = 0;
    while (1) {
        int c = fgetc(stream);
        if (c == EOF) {
            if (pos == 0) return -1;
            break;
        }
        if (pos + 2 >= *n) {
            size_t new_n = *n * 2;
            char *new_ptr = (char *)realloc(*lineptr, new_n);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
            *n = new_n;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == '\n') break;
    }
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}

int vsscanf(const char *str, const char *format, va_list ap)
{
    if (!str || !format) return 0;

    const char *s = str;
    const char *f = format;
    int assigned = 0;

    while (*f) {
        if (*f == ' ' || *f == '\t' || *f == '\n' || *f == '\r') {
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            f++;
            continue;
        }

        if (*f != '%') {
            if (*s != *f) {
                break;
            }
            s++;
            f++;
            continue;
        }

        f++; /* skip '%' */
        int suppress = 0;
        if (*f == '*') {
            suppress = 1;
            f++;
        }

        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }

        int length_mod = 0; /* 1 = l, 2 = ll */
        if (*f == 'l') {
            length_mod = 1;
            f++;
            if (*f == 'l') {
                length_mod = 2;
                f++;
            }
        }

        char spec = *f++;
        if (spec != 'c') {
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        }

        if (!*s && spec != 'c') {
            break;
        }

        if (spec == 'c') {
            if (!*s) break;
            if (!suppress) {
                char *dst = va_arg(ap, char *);
                *dst = *s;
                assigned++;
            }
            s++;
        } else if (spec == 's') {
            char *dst = suppress ? NULL : va_arg(ap, char *);
            int max_w = (width > 0) ? width : 1000000;
            int count = 0;
            while (*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' && count < max_w) {
                if (dst) dst[count] = *s;
                count++;
                s++;
            }
            if (dst && count > 0) {
                dst[count] = '\0';
                assigned++;
            }
        } else if (spec == 'd' || spec == 'i') {
            int sign = 1;
            if (*s == '-') { sign = -1; s++; }
            else if (*s == '+') { s++; }
            if (*s < '0' || *s > '9') break;
            long long val = 0;
            int count = 0;
            int max_w = (width > 0) ? width : 1000000;
            while (*s >= '0' && *s <= '9' && count < max_w) {
                val = val * 10 + (*s - '0');
                s++;
                count++;
            }
            val *= sign;
            if (!suppress) {
                if (length_mod == 2) {
                    *(long long *)va_arg(ap, long long *) = val;
                } else if (length_mod == 1) {
                    *(long *)va_arg(ap, long *) = (long)val;
                } else {
                    *(int *)va_arg(ap, int *) = (int)val;
                }
                assigned++;
            }
        } else if (spec == 'u') {
            if (*s < '0' || *s > '9') break;
            unsigned long long val = 0;
            int count = 0;
            int max_w = (width > 0) ? width : 1000000;
            while (*s >= '0' && *s <= '9' && count < max_w) {
                val = val * 10 + (*s - '0');
                s++;
                count++;
            }
            if (!suppress) {
                if (length_mod == 2) {
                    *(unsigned long long *)va_arg(ap, unsigned long long *) = val;
                } else if (length_mod == 1) {
                    *(unsigned long *)va_arg(ap, unsigned long *) = (unsigned long)val;
                } else {
                    *(unsigned int *)va_arg(ap, unsigned int *) = (unsigned int)val;
                }
                assigned++;
            }
        } else if (spec == 'x' || spec == 'X') {
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
            unsigned long long val = 0;
            int count = 0;
            int max_w = (width > 0) ? width : 1000000;
            int has_digits = 0;
            while (count < max_w) {
                char c = *s;
                int d = -1;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                val = (val << 4) | (unsigned long long)d;
                s++;
                count++;
                has_digits = 1;
            }
            if (!has_digits) break;
            if (!suppress) {
                if (length_mod == 2) {
                    *(unsigned long long *)va_arg(ap, unsigned long long *) = val;
                } else if (length_mod == 1) {
                    *(unsigned long *)va_arg(ap, unsigned long *) = (unsigned long)val;
                } else {
                    *(unsigned int *)va_arg(ap, unsigned int *) = (unsigned int)val;
                }
                assigned++;
            }
        } else {
            break;
        }
    }

    return assigned;
}

int sscanf(const char *str, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsscanf(str, format, ap);
    va_end(ap);
    return ret;
}

