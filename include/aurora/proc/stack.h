#ifndef _AURORA_PROC_STACK_H
#define _AURORA_PROC_STACK_H

#include <aurora/types.h>

uint64_t setup_user_stack(uint64_t stack_top_virt, int argc, char **argv, char **envp, uint64_t entry_point);

#endif /* _AURORA_PROC_STACK_H */
