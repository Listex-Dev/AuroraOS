#include <pwd.h>
#include <grp.h>
#include <string.h>

static struct passwd static_pwd = {
    .pw_name = "root",
    .pw_passwd = "x",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_gecos = "root",
    .pw_dir = "/root",
    .pw_shell = "/bin/sh"
};

struct passwd *getpwnam(const char *name)
{
    if (strcmp(name, "root") == 0) return &static_pwd;
    return NULL;
}

struct passwd *getpwuid(uid_t uid)
{
    if (uid == 0) return &static_pwd;
    return NULL;
}

static struct group static_grp = {
    .gr_name = "root",
    .gr_passwd = "x",
    .gr_gid = 0,
    .gr_mem = NULL
};

struct group *getgrnam(const char *name)
{
    if (strcmp(name, "root") == 0) return &static_grp;
    return NULL;
}

struct group *getgrgid(gid_t gid)
{
    if (gid == 0) return &static_grp;
    return NULL;
}
