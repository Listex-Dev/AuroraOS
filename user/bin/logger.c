/*
 * AuroraOS /bin/logger - Make entries in the system log
 *
 * Usage: logger [-p priority] [-t tag] [message...]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

static int parse_priority(const char *s)
{
    int fac = LOG_USER;
    int pri = LOG_NOTICE;

    char buf[64];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *dot = strchr(buf, '.');
    if (dot) {
        *dot = '\0';
        char *fstr = buf;
        char *pstr = dot + 1;

        if (strcmp(fstr, "auth") == 0 || strcmp(fstr, "authpriv") == 0) fac = LOG_AUTH;
        else if (strcmp(fstr, "daemon") == 0) fac = LOG_DAEMON;
        else if (strcmp(fstr, "cron") == 0) fac = LOG_CRON;
        else if (strcmp(fstr, "kern") == 0) fac = LOG_KERN;
        else if (strcmp(fstr, "user") == 0) fac = LOG_USER;

        if (strcmp(pstr, "emerg") == 0) pri = LOG_EMERG;
        else if (strcmp(pstr, "alert") == 0) pri = LOG_ALERT;
        else if (strcmp(pstr, "crit") == 0) pri = LOG_CRIT;
        else if (strcmp(pstr, "err") == 0) pri = LOG_ERR;
        else if (strcmp(pstr, "warning") == 0 || strcmp(pstr, "warn") == 0) pri = LOG_WARNING;
        else if (strcmp(pstr, "notice") == 0) pri = LOG_NOTICE;
        else if (strcmp(pstr, "info") == 0) pri = LOG_INFO;
        else if (strcmp(pstr, "debug") == 0) pri = LOG_DEBUG;

        return fac | pri;
    }

    if (strcmp(s, "emerg") == 0) return fac | LOG_EMERG;
    if (strcmp(s, "alert") == 0) return fac | LOG_ALERT;
    if (strcmp(s, "crit") == 0) return fac | LOG_CRIT;
    if (strcmp(s, "err") == 0) return fac | LOG_ERR;
    if (strcmp(s, "warning") == 0 || strcmp(s, "warn") == 0) return fac | LOG_WARNING;
    if (strcmp(s, "notice") == 0) return fac | LOG_NOTICE;
    if (strcmp(s, "info") == 0) return fac | LOG_INFO;
    if (strcmp(s, "debug") == 0) return fac | LOG_DEBUG;

    return fac | pri;
}

int main(int argc, char **argv)
{
    const char *tag = "logger";
    int priority = LOG_USER | LOG_NOTICE;
    int msg_start = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            tag = argv[++i];
            msg_start = i + 1;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            priority = parse_priority(argv[++i]);
            msg_start = i + 1;
        } else if (argv[i][0] != '-') {
            msg_start = i;
            break;
        }
    }

    char message[1024];
    message[0] = '\0';

    if (msg_start < argc) {
        size_t len = 0;
        for (int i = msg_start; i < argc; i++) {
            if (i > msg_start && len < sizeof(message) - 1) {
                message[len++] = ' ';
            }
            size_t slen = strlen(argv[i]);
            if (len + slen < sizeof(message) - 1) {
                memcpy(&message[len], argv[i], slen);
                len += slen;
            }
        }
        message[len] = '\0';
    } else {
        /* Read from stdin only if not a tty */
        if (isatty(STDIN_FILENO)) {
            return 0;
        }
        if (!fgets(message, sizeof(message), stdin)) {
            return 0;
        }
        size_t l = strlen(message);
        if (l > 0 && message[l - 1] == '\n') message[l - 1] = '\0';
    }

    openlog(tag, LOG_PID, priority & LOG_FACMASK);
    syslog(priority, "%s", message);
    closelog();

    return 0;
}
