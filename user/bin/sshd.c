/*
 * AuroraOS /sbin/sshd - Secure Shell Remote Access Daemon
 *
 * Responsibilities:
 * - Creates /run/sshd.pid
 * - Listens on TCP port 22
 * - Sends SSH-2.0 banner
 * - Authenticates against /etc/shadow using crypt()
 * - Allocates PTY pair (/dev/ptmx -> /dev/pts/N)
 * - Spawns /bin/sh on the slave PTY
 * - Bridges TCP socket <-> master PTY
 * - Logs authentication and session events to /var/log/auth.log
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <errno.h>

#define PID_FILE "/run/sshd.pid"
#define LOG_AUTH "/var/log/auth.log"

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

static void log_auth(const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[32];
    if (tm) {
        strftime(time_str, sizeof(time_str), "%b %e %T", tm);
    } else {
        strcpy(time_str, "Oct 19 12:00:00");
    }

    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    char line[1280];
    snprintf(line, sizeof(line), "%s aurora sshd[%d]: %s\n", time_str, getpid(), msg);

    int fd = open(LOG_AUTH, O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (fd >= 0) {
        write(fd, line, strlen(line));
        close(fd);
    }
}

static ssize_t read_line(int fd, char *buf, size_t max_len, int do_echo)
{
    size_t pos = 0;
    while (pos < max_len - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            if (pos == 0) return -1;
            break;
        }
        if (c == '\r') {
            int avail = 0;
            if (ioctl(fd, FIONREAD, &avail) == 0 && avail > 0) {
                char next_c;
                read(fd, &next_c, 1);
            }
            write(fd, "\r\n", 2);
            break;
        }
        if (c == '\n') {
            write(fd, "\r\n", 2);
            break;
        }
        if (c == '\b' || (unsigned char)c == 0x7F) {
            if (pos > 0) {
                pos--;
                if (do_echo) write(fd, "\b \b", 3);
            }
            continue;
        }
        buf[pos++] = c;
        if (do_echo) {
            write(fd, &c, 1);
        }
    }
    buf[pos] = '\0';
    return (ssize_t)pos;
}

static void handle_client(int client_fd, struct sockaddr_in *client_addr)
{
    char client_ip[32];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr->sin_port);

    log_auth("Connection from %s port %d", client_ip, client_port);

    /* 1. Send SSH identification banner */
    const char *banner = "SSH-2.0-AuroraSSH_1.0\r\n";
    write(client_fd, banner, strlen(banner));

    /* 2. Interactive login prompt */
    const char *prompt_user = "aurora login: ";
    write(client_fd, prompt_user, strlen(prompt_user));

    char username[64];
    if (read_line(client_fd, username, sizeof(username), 1) <= 0) {
        close(client_fd);
        _exit(1);
    }

    const char *prompt_pass = "Password: ";
    write(client_fd, prompt_pass, strlen(prompt_pass));

    char password[64];
    if (read_line(client_fd, password, sizeof(password), 0) < 0) {
        close(client_fd);
        _exit(1);
    }

    /* 3. Verify against /etc/shadow */
    int auth_ok = 0;
    struct spwd *sp = getspnam(username);
    if (sp && sp->sp_pwdp) {
        char *hash = crypt(password, sp->sp_pwdp);
        if (hash && strcmp(hash, sp->sp_pwdp) == 0) {
            auth_ok = 1;
        }
    }

    if (!auth_ok) {
        log_auth("Failed password for %s from %s port %d", username, client_ip, client_port);
        const char *fail_msg = "\r\nAuthentication failed.\r\n";
        write(client_fd, fail_msg, strlen(fail_msg));
        close(client_fd);
        _exit(1);
    }

    log_auth("Accepted password for %s from %s port %d", username, client_ip, client_port);

    const char *welcome = "\r\nWelcome to AuroraOS 2.0 Ultra (x86_64 remote terminal)!\r\n\r\n";
    write(client_fd, welcome, strlen(welcome));

    /* 4. Allocate PTY pair */
    int master_fd = -1;
    int slave_fd = -1;
    char pts_name[64];
    if (openpty(&master_fd, &slave_fd, pts_name, NULL, NULL) < 0) {
        log_auth("Failed to allocate PTY: %s", strerror(errno));
        close(client_fd);
        _exit(1);
    }

    log_auth("Allocated PTY %s for %s", pts_name, username);

    /* 5. Spawn shell child */
    pid_t shell_pid = fork();
    if (shell_pid < 0) {
        log_auth("fork failed for shell: %s", strerror(errno));
        close(master_fd);
        close(slave_fd);
        close(client_fd);
        _exit(1);
    }

    if (shell_pid == 0) {
        /* Shell child */
        close(master_fd);
        close(client_fd);

        setsid();
        ioctl(slave_fd, TIOCSCTTY, 0);

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);

        struct passwd *pw = getpwnam(username);
        if (pw) {
            setgid(pw->pw_gid);
            setuid(pw->pw_uid);
            setenv("HOME", pw->pw_dir, 1);
        } else {
            setenv("HOME", (strcmp(username, "root") == 0) ? "/root" : "/home/aurora", 1);
        }

        setenv("USER", username, 1);
        setenv("TERM", "linux", 1);
        setenv("SHELL", "/bin/sh", 1);
        setenv("PATH", "/bin:/usr/bin:/sbin", 1);

        char *sh_argv[] = { "/bin/sh", NULL };
        execve("/bin/sh", sh_argv, environ);
        _exit(127);
    }

    /* 6. Parent bridge loop: client_fd <-> master_fd */
    close(slave_fd);

    while (1) {
        int bytes_sock = 0;
        if (ioctl(client_fd, FIONREAD, &bytes_sock) == 0 && bytes_sock > 0) {
            char buf[512];
            ssize_t rd = read(client_fd, buf, sizeof(buf));
            if (rd <= 0) break;
            write(master_fd, buf, rd);
        }

        int bytes_pty = 0;
        if (ioctl(master_fd, FIONREAD, &bytes_pty) == 0 && bytes_pty > 0) {
            char buf[512];
            ssize_t rd = read(master_fd, buf, sizeof(buf));
            if (rd <= 0) break;
            write(client_fd, buf, rd);
        }

        int status;
        if (waitpid(shell_pid, &status, WNOHANG) > 0) {
            /* Drain any remaining pty output */
            ioctl(master_fd, FIONREAD, &bytes_pty);
            if (bytes_pty > 0) {
                char buf[512];
                ssize_t rd = read(master_fd, buf, sizeof(buf));
                if (rd > 0) write(client_fd, buf, rd);
            }
            break;
        }

        usleep(5000);
    }

    log_auth("Session closed for %s", username);

    close(master_fd);
    close(client_fd);
    _exit(0);
}

int main(int argc, char **argv)
{
    int foreground = 0;
    int port = 22;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "-f") == 0) {
            foreground = 1;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    if (!foreground) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("sshd: fork");
            return 1;
        }
        if (pid > 0) {
            return 0;
        }
        setsid();
    }

    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    write_pid_file();

    mkdir("/var", 0755);
    mkdir("/var/log", 0755);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("sshd: socket");
        remove_pid_file();
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("sshd: bind");
        close(listen_fd);
        remove_pid_file();
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("sshd: listen");
        close(listen_fd);
        remove_pid_file();
        return 1;
    }

    log_auth("Server listening on 0.0.0.0 port %d", port);

    while (g_running) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client, &clen);
        if (client_fd >= 0) {
            pid_t pid = fork();
            if (pid == 0) {
                close(listen_fd);
                handle_client(client_fd, &client);
            }
            close(client_fd);
        } else {
            if (errno == EINTR) continue;
            sleep(1);
        }

        /* Reap zombies */
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {
        }
    }

    log_auth("Server shutting down");
    close(listen_fd);
    remove_pid_file();
    return 0;
}
