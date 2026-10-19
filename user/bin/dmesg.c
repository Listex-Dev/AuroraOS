/*
 * AuroraOS Kernel Ring Buffer Viewer (dmesg)
 * Displays kernel diagnostic and boot logs using klogctl/syslog.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define SYS_syslog 103
#define SYSLOG_ACTION_READ_ALL 3

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char *buf = malloc(65536);
    if (!buf) {
        fprintf(stderr, "dmesg: out of memory\n");
        return 1;
    }

    long n = syscall(SYS_syslog, SYSLOG_ACTION_READ_ALL, buf, 65535);
    if (n > 0) {
        buf[n] = '\0';
        fputs(buf, stdout);
        free(buf);
        return 0;
    }

    /* Fallback: try reading /var/log/messages */
    FILE *f = fopen("/var/log/messages", "r");
    if (f) {
        size_t rd = fread(buf, 1, 65535, f);
        fclose(f);
        if (rd > 0) {
            buf[rd] = '\0';
            fputs(buf, stdout);
            free(buf);
            return 0;
        }
    }

    free(buf);
    fprintf(stderr, "dmesg: klogctl failed\n");
    return 1;
}
