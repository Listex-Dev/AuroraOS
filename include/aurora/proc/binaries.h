#ifndef _AURORA_PROC_BINARIES_H
#define _AURORA_PROC_BINARIES_H

#include <aurora/types.h>

const void *find_builtin_binary(const char *path, size_t *size_out);
const void *find_builtin_binary_meta(const char *path, size_t *size_out, mode_t *mode_out, uid_t *uid_out, gid_t *gid_out);

#endif /* _AURORA_PROC_BINARIES_H */
