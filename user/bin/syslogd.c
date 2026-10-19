/*
 * AuroraOS /sbin/syslogd - System Logging Daemon
 *
 * Responsibilities:
 * - Creates /run/syslogd.pid
 * - Listens for UDP syslog packets on port 514
 * - Routes general logs to /var/log/messages
 * - Routes security/auth logs to /var/log/auth.log
 * - Supports graceful shutdown via SIGTERM / SIGINT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>

#define PID_FILE "/run/syslogd.pid"
#define FILE_LOG_MESSAGES "/var/log/messages"
#define FILE_LOG_AUTH "/var/log/auth.log"

static volatile int g_running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void write_pid_file(void)
{
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d\n", getpid());
        write(fd, buf, strlen(buf));
        close(fd);
    }
}

static void remove_pid_file(void)
{
    unlink(PID_FILE);
}

static void log_to_file(const char *path, const char *msg)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        if (msg[strlen(msg) - 1] != '\n') {
            write(fd, "\n", 1);
        }
        close(fd);
    }
}

static void format_and_dispatch(const char *raw_buf, ssize_t len)
{
    char buf[2048];
    if (len >= (ssize_t)sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, raw_buf, len);
    buf[len] = '\0';

    int pri = LOG_USER | LOG_INFO;
    char *msg_ptr = buf;

    /* Parse <PRI> prefix if present */
    if (buf[0] == '<') {
        char *end = strchr(buf, '>');
        if (end) {
            pri = atoi(&buf[1]);
            msg_ptr = end + 1;
        }
    }

    int fac = LOG_FAC(pri);

    /* Construct formatted syslog entry: "Mmm dd hh:mm:ss aurora <message>" */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[32];
    if (tm) {
        strftime(time_str, sizeof(time_str), "%b %e %T", tm);
    } else {
        strcpy(time_str, "Oct 19 12:00:00");
    }

    char entry[2560];
    snprintf(entry, sizeof(entry), "%s aurora %s", time_str, msg_ptr);

    /* Check facility for auth/authpriv */
    int is_auth = (fac == (LOG_AUTH >> 3) || fac == (LOG_AUTHPRIV >> 3));
    if (strstr(msg_ptr, "auth") || strstr(msg_ptr, "login") ||
        strstr(msg_ptr, "su:") || strstr(msg_ptr, "sshd")) {
        is_auth = 1;
    }

    if (is_auth) {
        log_to_file(FILE_LOG_AUTH, entry);
    }
    log_to_file(FILE_LOG_MESSAGES, entry);
}

int main(int argc, char **argv)
{
    int foreground = 0;
    int port = 514;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-F") == 0) {
            foreground = 1;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    if (!foreground) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("syslogd: fork");
            return 1;
        }
        if (pid > 0) {
            /* Parent exits */
            return 0;
        }
        setsid();
    }

    /* Signal setup */
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);

    write_pid_file();

    /* Ensure log directories exist */
    mkdir("/var", 0755);
    mkdir("/var/log", 0755);

    /* Startup log */
    char start_msg[128];
    snprintf(start_msg, sizeof(start_msg), "<%d>syslogd[%d]: [INFO] System logging daemon started",
             LOG_DAEMON | LOG_INFO, getpid());
    format_and_dispatch(start_msg, strlen(start_msg));

    /* Open UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("syslogd: socket");
        remove_pid_file();
        return 1;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("syslogd: bind");
        close(sock);
        remove_pid_file();
        return 1;
    }

    char recv_buf[2048];
    while (g_running) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        ssize_t n = recvfrom(sock, recv_buf, sizeof(recv_buf) - 1, 0, (struct sockaddr *)&client, &clen);
        if (n > 0) {
            recv_buf[n] = '\0';
            format_and_dispatch(recv_buf, n);
        } else {
            if (errno == EINTR) {
                continue;
            }
            sleep(1);
        }
    }

    char stop_msg[128];
    snprintf(stop_msg, sizeof(stop_msg), "<%d>syslogd[%d]: [INFO] System logging daemon stopping",
             LOG_DAEMON | LOG_INFO, getpid());
    format_and_dispatch(stop_msg, strlen(stop_msg));

    close(sock);
    remove_pid_file();
    return 0;
}
