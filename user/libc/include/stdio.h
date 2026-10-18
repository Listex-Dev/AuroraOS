#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#define EOF (-1)
#define BUFSIZ 1024

#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct _FILE {
    int   fd;
    int   flags;
    int   eof;
    int   error;
    char *buf;
    size_t buf_size;
    size_t buf_pos;
    size_t buf_len;
    char  unget[4];
    int   unget_count;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define stdin  stdin
#define stdout stdout
#define stderr stderr

FILE *fopen(const char *pathname, const char *mode);
FILE *fdopen(int fd, const char *mode);
int   fclose(FILE *stream);
int   fflush(FILE *stream);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);

int getchar(void);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);

int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
void perror(const char *s);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

#endif /* _STDIO_H */
