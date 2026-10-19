#ifndef _SHADOW_H
#define _SHADOW_H

#include <sys/types.h>

struct spwd {
    char *sp_namp; /* Login name */
    char *sp_pwdp; /* Encrypted password */
    long  sp_lstchg;
    long  sp_min;
    long  sp_max;
    long  sp_warn;
    long  sp_inact;
    long  sp_expire;
    unsigned long sp_flag;
};

struct spwd *getspnam(const char *name);

#endif /* _SHADOW_H */
