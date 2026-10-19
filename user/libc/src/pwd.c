#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static struct passwd g_pwd;
static char g_pw_line[256];

static struct passwd *parse_passwd_line(char *line)
{
    /* Format: username:passwd:uid:gid:gecos:dir:shell */
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }

    char *tokens[7];
    int idx = 0;
    char *p = line;
    tokens[idx++] = p;

    while (*p && idx < 7) {
        if (*p == ':') {
            *p = '\0';
            tokens[idx++] = p + 1;
        }
        p++;
    }

    if (idx < 7) {
        return NULL;
    }

    g_pwd.pw_name = tokens[0];
    g_pwd.pw_passwd = tokens[1];
    g_pwd.pw_uid = (uid_t)atoi(tokens[2]);
    g_pwd.pw_gid = (gid_t)atoi(tokens[3]);
    g_pwd.pw_gecos = tokens[4];
    g_pwd.pw_dir = tokens[5];
    g_pwd.pw_shell = tokens[6];

    return &g_pwd;
}

struct passwd *getpwnam(const char *name)
{
    if (!name) return NULL;

    FILE *fp = fopen("/etc/passwd", "r");
    if (fp) {
        while (fgets(g_pw_line, sizeof(g_pw_line), fp)) {
            char copy[256];
            strncpy(copy, g_pw_line, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';
            struct passwd *pw = parse_passwd_line(copy);
            if (pw && strcmp(pw->pw_name, name) == 0) {
                /* Reparse into persistent g_pw_line */
                fclose(fp);
                return parse_passwd_line(g_pw_line);
            }
        }
        fclose(fp);
    }

    /* Fallback default entries */
    if (strcmp(name, "root") == 0) {
        g_pwd.pw_name = "root";
        g_pwd.pw_passwd = "x";
        g_pwd.pw_uid = 0;
        g_pwd.pw_gid = 0;
        g_pwd.pw_gecos = "root";
        g_pwd.pw_dir = "/root";
        g_pwd.pw_shell = "/bin/sh";
        return &g_pwd;
    }
    if (strcmp(name, "aurora") == 0) {
        g_pwd.pw_name = "aurora";
        g_pwd.pw_passwd = "x";
        g_pwd.pw_uid = 1000;
        g_pwd.pw_gid = 1000;
        g_pwd.pw_gecos = "Aurora User";
        g_pwd.pw_dir = "/home/aurora";
        g_pwd.pw_shell = "/bin/sh";
        return &g_pwd;
    }

    return NULL;
}

struct passwd *getpwuid(uid_t uid)
{
    FILE *fp = fopen("/etc/passwd", "r");
    if (fp) {
        while (fgets(g_pw_line, sizeof(g_pw_line), fp)) {
            char copy[256];
            strncpy(copy, g_pw_line, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';
            struct passwd *pw = parse_passwd_line(copy);
            if (pw && pw->pw_uid == uid) {
                fclose(fp);
                return parse_passwd_line(g_pw_line);
            }
        }
        fclose(fp);
    }

    if (uid == 0) return getpwnam("root");
    if (uid == 1000) return getpwnam("aurora");
    return NULL;
}

static struct spwd g_spwd;
static char g_sp_line[512];

struct spwd *getspnam(const char *name)
{
    if (!name) return NULL;

    /* Use open() directly so access checks are enforced via VFS */
    int fd = open("/etc/shadow", O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    char buf[2048];
    ssize_t rd = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (rd <= 0) {
        return NULL;
    }
    buf[rd] = '\0';

    char *line = buf;
    while (*line) {
        char *eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        strncpy(g_sp_line, line, sizeof(g_sp_line) - 1);
        g_sp_line[sizeof(g_sp_line) - 1] = '\0';

        char *colon1 = strchr(g_sp_line, ':');
        if (colon1) {
            *colon1 = '\0';
            if (strcmp(g_sp_line, name) == 0) {
                char *colon2 = strchr(colon1 + 1, ':');
                if (colon2) *colon2 = '\0';

                g_spwd.sp_namp = g_sp_line;
                g_spwd.sp_pwdp = colon1 + 1;
                g_spwd.sp_lstchg = 19000;
                g_spwd.sp_min = 0;
                g_spwd.sp_max = 99999;
                g_spwd.sp_warn = 7;
                g_spwd.sp_inact = -1;
                g_spwd.sp_expire = -1;
                g_spwd.sp_flag = 0;
                return &g_spwd;
            }
        }

        if (!eol) break;
        line = eol + 1;
    }

    return NULL;
}

static struct group g_grp;
static char g_gr_line[256];

struct group *getgrnam(const char *name)
{
    if (!name) return NULL;

    FILE *fp = fopen("/etc/group", "r");
    if (fp) {
        while (fgets(g_gr_line, sizeof(g_gr_line), fp)) {
            char copy[256];
            strncpy(copy, g_gr_line, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';

            char *c1 = strchr(copy, ':');
            if (c1) {
                *c1 = '\0';
                if (strcmp(copy, name) == 0) {
                    char *c2 = strchr(c1 + 1, ':');
                    char *c3 = c2 ? strchr(c2 + 1, ':') : NULL;
                    if (c2) *c2 = '\0';
                    if (c3) *c3 = '\0';

                    g_grp.gr_name = (char *)name;
                    g_grp.gr_passwd = c1 + 1;
                    g_grp.gr_gid = c2 ? (gid_t)atoi(c2 + 1) : 0;
                    g_grp.gr_mem = NULL;
                    fclose(fp);
                    return &g_grp;
                }
            }
        }
        fclose(fp);
    }

    if (strcmp(name, "root") == 0) {
        g_grp.gr_name = "root";
        g_grp.gr_passwd = "x";
        g_grp.gr_gid = 0;
        g_grp.gr_mem = NULL;
        return &g_grp;
    }
    if (strcmp(name, "users") == 0) {
        g_grp.gr_name = "users";
        g_grp.gr_passwd = "x";
        g_grp.gr_gid = 1000;
        g_grp.gr_mem = NULL;
        return &g_grp;
    }

    return NULL;
}

struct group *getgrgid(gid_t gid)
{
    if (gid == 0) return getgrnam("root");
    if (gid == 1000) return getgrnam("users");
    return NULL;
}
