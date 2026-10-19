#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static char s_ident[64] = "aurora";
static int s_logopt = 0;
static int s_facility = LOG_USER;
static int s_syslog_fd = -1;

void openlog(const char *ident, int option, int facility)
{
    if (ident) {
        strncpy(s_ident, ident, sizeof(s_ident) - 1);
        s_ident[sizeof(s_ident) - 1] = '\0';
    }
    s_logopt = option;
    s_facility = facility ? facility : LOG_USER;
}

void closelog(void)
{
    if (s_syslog_fd >= 0) {
        close(s_syslog_fd);
        s_syslog_fd = -1;
    }
}

void syslog(int priority, const char *format, ...)
{
    char msg[1024];
    va_list ap;
    va_start(ap, format);
    vsnprintf(msg, sizeof(msg), format, ap);
    va_end(ap);

    int pri = priority;
    if ((pri & LOG_FACMASK) == 0) {
        pri |= s_facility;
    }

    char packet[1280];
    if (s_logopt & LOG_PID) {
        snprintf(packet, sizeof(packet), "<%d>%s[%d]: %s", pri, s_ident, getpid(), msg);
    } else {
        snprintf(packet, sizeof(packet), "<%d>%s: %s", pri, s_ident, msg);
    }

    /* Try sending via UDP to 127.0.0.1:514 */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int sent = -1;
    if (sock >= 0) {
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(514);
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sent = sendto(sock, packet, strlen(packet), 0, (struct sockaddr *)&sin, sizeof(sin));
        close(sock);
    }

    /* Fallback directly to log files if daemon isn't reachable */
    if (sent <= 0) {
        int fac = LOG_FAC(pri);
        const char *log_file = (fac == (LOG_AUTH >> 3) || fac == (LOG_AUTHPRIV >> 3))
                               ? "/var/log/auth.log" : "/var/log/messages";
        int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0640);
        if (fd >= 0) {
            char line[1536];
            snprintf(line, sizeof(line), "%s\n", packet);
            write(fd, line, strlen(line));
            close(fd);
        }
    }

    if (s_logopt & LOG_PERROR) {
        fprintf(stderr, "%s\n", packet);
    }
}
