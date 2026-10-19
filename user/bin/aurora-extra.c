/*
 * AuroraOS Extra Utility (Sample Addon Package for M17)
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    struct utsname u;
    if (uname(&u) == 0) {
        printf("AuroraOS Extra Utility [v1.0.0]\n");
        printf("OS: %s %s (%s)\n", u.sysname, u.release, u.machine);
    } else {
        printf("AuroraOS Extra Utility [v1.0.0] operational.\n");
    }
    printf("Package installation verified successfully!\n");
    return 0;
}
