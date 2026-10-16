#include <aurora/proc/stack.h>
#include <aurora/elf64.h>
#include <aurora/string.h>

uint64_t setup_user_stack(uint64_t stack_top_virt, int argc, char **argv, char **envp, uint64_t entry_point)
{
    uint8_t *sp = (uint8_t *)stack_top_virt;

    /* 1. Copy argument strings and store user pointers */
    uint64_t argv_ptrs[16];
    int actual_argc = argc > 15 ? 15 : argc;

    for (int i = actual_argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        memcpy(sp, argv[i], len);
        argv_ptrs[i] = (uint64_t)sp;
    }

    /* Copy environment strings */
    uint64_t envp_ptrs[16];
    int envc = 0;
    if (envp) {
        while (envp[envc] && envc < 15) {
            envc++;
        }
        for (int i = envc - 1; i >= 0; i--) {
            size_t len = strlen(envp[i]) + 1;
            sp -= len;
            memcpy(sp, envp[i], len);
            envp_ptrs[i] = (uint64_t)sp;
        }
    }

    /* 16-byte stack alignment */
    sp = (uint8_t *)((uintptr_t)sp & ~0xFULL);

    uint64_t *stack = (uint64_t *)sp;

    /* 2. Push Auxiliary Vector (auxv) */
    *(--stack) = 0;
    *(--stack) = AT_NULL;

    *(--stack) = entry_point;
    *(--stack) = AT_ENTRY;

    *(--stack) = 4096;
    *(--stack) = AT_PAGESZ;

    *(--stack) = 0;
    *(--stack) = AT_UID;

    *(--stack) = 0;
    *(--stack) = AT_EUID;

    *(--stack) = 0;
    *(--stack) = AT_GID;

    *(--stack) = 0;
    *(--stack) = AT_EGID;

    /* 3. Push envp NULL and pointers */
    *(--stack) = 0;
    for (int i = envc - 1; i >= 0; i--) {
        *(--stack) = envp_ptrs[i];
    }

    /* 4. Push argv NULL and pointers */
    *(--stack) = 0;
    for (int i = actual_argc - 1; i >= 0; i--) {
        *(--stack) = argv_ptrs[i];
    }

    /* 5. Push argc */
    *(--stack) = (uint64_t)actual_argc;

    return (uint64_t)stack;
}
