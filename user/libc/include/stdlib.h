#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include <sys/types.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     2147483647

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

void  exit(int status);
void _Exit(int status);
void  abort(void);
int   atexit(void (*function)(void));

char *getenv(const char *name);
int   setenv(const char *name, const char *value, int overwrite);
int   unsetenv(const char *name);
int   system(const char *command);

int    atoi(const char *nptr);
long   atol(const char *nptr);
long   strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

int  rand(void);
void srand(unsigned int seed);
int  abs(int j);
long labs(long j);

#endif /* _STDLIB_H */
