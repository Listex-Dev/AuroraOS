/*
 * AuroraOS /bin/id - Print user and group IDs
 */

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uid_t uid = getuid();
    gid_t gid = getgid();
    uid_t euid = geteuid();
    gid_t egid = getegid();

    struct passwd *pw = getpwuid(uid);
    struct group *gr = getgrgid(gid);

    printf("uid=%u(%s) gid=%u(%s)",
           uid, pw ? pw->pw_name : "unknown",
           gid, gr ? gr->gr_name : "unknown");

    if (euid != uid) {
        struct passwd *epw = getpwuid(euid);
        printf(" euid=%u(%s)", euid, epw ? epw->pw_name : "unknown");
    }

    if (egid != gid) {
        struct group *egr = getgrgid(egid);
        printf(" egid=%u(%s)", egid, egr ? egr->gr_name : "unknown");
    }

    printf(" groups=%u(%s)\n", gid, gr ? gr->gr_name : "unknown");
    return 0;
}
