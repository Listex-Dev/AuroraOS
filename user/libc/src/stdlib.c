#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "syscall.h"

char **environ = NULL;

extern void __libc_init_stdio(void);

#define MAX_ATEXIT 32
static void (*atexit_funcs[MAX_ATEXIT])(void);
static int atexit_count = 0;

void __libc_start_main(int (*main_fn)(int, char **, char **), int argc, char **argv, char **envp)
{
    environ = envp;
    __libc_init_stdio();
    int ret = main_fn(argc, argv, envp);
    exit(ret);
}

int atexit(void (*function)(void))
{
    if (atexit_count < MAX_ATEXIT) {
        atexit_funcs[atexit_count++] = function;
        return 0;
    }
    return -1;
}

void exit(int status)
{
    while (atexit_count > 0) {
        atexit_funcs[--atexit_count]();
    }
    fflush(stdout);
    fflush(stderr);
    _exit(status);
}

void _Exit(int status)
{
    _exit(status);
}

void abort(void)
{
    fputs("Aborted\n", stderr);
    kill(getpid(), SIGABRT);
    _exit(134);
}

void __assert_fail(const char *expr, const char *file, int line, const char *func)
{
    fprintf(stderr, "Assertion failed: %s (%s: %s: %d)\n", expr, file, func ? func : "?", line);
    abort();
}

int atoi(const char *nptr)
{
    return (int)atol(nptr);
}

long atol(const char *nptr)
{
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;
    int sign = 1;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;
    long val = 0;
    while (*nptr >= '0' && *nptr <= '9') {
        val = val * 10 + (*nptr - '0');
        nptr++;
    }
    return sign * val;
}

long strtol(const char *nptr, char **endptr, int base)
{
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;
    int sign = 1;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') nptr++;

    if (base == 0) {
        if (*nptr == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
            base = 16;
            nptr += 2;
        } else if (*nptr == '0') {
            base = 8;
            nptr++;
        } else {
            base = 10;
        }
    } else if (base == 16 && *nptr == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
        nptr += 2;
    }

    long val = 0;
    while (*nptr) {
        int d = -1;
        if (*nptr >= '0' && *nptr <= '9') d = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'z') d = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'Z') d = *nptr - 'A' + 10;
        if (d < 0 || d >= base) break;
        val = val * base + d;
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return sign * val;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long)strtol(nptr, endptr, base);
}

char *getenv(const char *name)
{
    if (!environ || !name) return NULL;
    size_t nlen = strlen(name);
    for (char **ep = environ; *ep != NULL; ep++) {
        if (strncmp(*ep, name, nlen) == 0 && (*ep)[nlen] == '=') {
            return *ep + nlen + 1;
        }
    }
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || *name == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!value) value = "";

    size_t nlen = strlen(name);
    size_t vlen = strlen(value);
    char *new_entry = malloc(nlen + 1 + vlen + 1);
    if (!new_entry) {
        errno = ENOMEM;
        return -1;
    }
    memcpy(new_entry, name, nlen);
    new_entry[nlen] = '=';
    memcpy(new_entry + nlen + 1, value, vlen);
    new_entry[nlen + 1 + vlen] = '\0';

    if (environ) {
        for (int i = 0; environ[i] != NULL; i++) {
            if (strncmp(environ[i], name, nlen) == 0 && environ[i][nlen] == '=') {
                if (!overwrite) {
                    free(new_entry);
                    return 0;
                }
                environ[i] = new_entry;
                return 0;
            }
        }
    }

    int count = 0;
    if (environ) {
        while (environ[count]) count++;
    }

    char **new_env = malloc((count + 2) * sizeof(char *));
    if (!new_env) {
        free(new_entry);
        errno = ENOMEM;
        return -1;
    }

    for (int i = 0; i < count; i++) {
        new_env[i] = environ[i];
    }
    new_env[count] = new_entry;
    new_env[count + 1] = NULL;
    environ = new_env;
    return 0;
}

int unsetenv(const char *name)
{
    if (!name || *name == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (!environ) return 0;

    size_t nlen = strlen(name);
    int i = 0;
    while (environ[i]) {
        if (strncmp(environ[i], name, nlen) == 0 && environ[i][nlen] == '=') {
            int j = i;
            while (environ[j]) {
                environ[j] = environ[j + 1];
                j++;
            }
        } else {
            i++;
        }
    }
    return 0;
}

int system(const char *command)
{
    if (!command) return 1;
    pid_t pid = fork();
    if (pid == 0) {
        char *const argv[] = { "/bin/sh", "-c", (char *)command, NULL };
        execve("/bin/sh", argv, environ);
        _exit(127);
    }
    if (pid < 0) return -1;
    int status = 0;
    waitpid(pid, &status, 0);
    return status;
}

static unsigned int next_rand = 1;

int rand(void)
{
    next_rand = next_rand * 1103515245 + 12345;
    return (unsigned int)(next_rand / 65536) % 32768;
}

void srand(unsigned int seed)
{
    next_rand = seed;
}

int abs(int j)
{
    return (j < 0) ? -j : j;
}

long labs(long j)
{
    return (j < 0) ? -j : j;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    if (nmemb <= 1) return;
    char *arr = (char *)base;
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - 1 - i; j++) {
            if (compar(arr + j * size, arr + (j + 1) * size) > 0) {
                for (size_t k = 0; k < size; k++) {
                    char tmp = arr[j * size + k];
                    arr[j * size + k] = arr[(j + 1) * size + k];
                    arr[(j + 1) * size + k] = tmp;
                }
            }
        }
    }
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    size_t l = 0, r = nmemb;
    const char *arr = (const char *)base;
    while (l < r) {
        size_t mid = l + (r - l) / 2;
        int cmp = compar(key, arr + mid * size);
        if (cmp == 0) return (void *)(arr + mid * size);
        if (cmp < 0) r = mid;
        else l = mid + 1;
    }
    return NULL;
}
