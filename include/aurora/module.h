#ifndef _AURORA_MODULE_H
#define _AURORA_MODULE_H

#include <aurora/types.h>

#define MODULE_NAME_LEN 64

enum module_state {
    MODULE_STATE_LIVE = 0,
    MODULE_STATE_COMING,
    MODULE_STATE_GOING
};

typedef int (*module_init_fn)(void);
typedef void (*module_cleanup_fn)(void);

struct module {
    char name[MODULE_NAME_LEN];
    enum module_state state;
    void *core_base;            /* Base virtual address of allocated module memory */
    size_t core_size;           /* Total size allocated */
    module_init_fn init;        /* init_module entry point */
    module_cleanup_fn cleanup;  /* cleanup_module entry point */
    int refcnt;                 /* Reference counter */
    struct module *next;
};

struct kernel_symbol {
    const char *name;
    void *addr;
};

/* Module subsystem API */
void module_subsys_init(void);
void *kallsyms_lookup_name(const char *name);
struct module *module_find(const char *name);
struct module *module_get_list(void);
void *module_alloc(size_t size);
void module_free(void *ptr, size_t size);
size_t module_format_proc_modules(char *buf, size_t max_len);

/* Syscall entry points */
int64_t sys_init_module(void *umod, unsigned long len, const char *uargs);
int64_t sys_delete_module(const char *name, unsigned int flags);

#endif /* _AURORA_MODULE_H */
