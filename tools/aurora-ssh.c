/*
 * AuroraOS Host SSH / Remote Terminal Client
 *
 * Connects to AuroraOS running in QEMU on port 2222.
 * Sets terminal to raw mode so arrow keys, Ctrl-C, tab, history,
 * vi, and job control all work naturally.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <termios.h>
#include <errno.h>

static struct termios orig_termios;
static int raw_mode_set = 0;

static void reset_terminal(void)
{
    if (raw_mode_set) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        raw_mode_set = 0;
    }
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    int port = 2222;

    if (argc > 1 && strcmp(argv[1], "-h") != 0 && strcmp(argv[1], "--help") != 0) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }

    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: %s [host] [port]\n", argv[0]);
        printf("Default: 127.0.0.1 2222\n");
        return 0;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &sin.sin_addr) <= 0) {
        fprintf(stderr, "Invalid host address: %s\n", host);
        return 1;
    }

    printf("Connecting to AuroraOS at %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "\n[ERROR] Connection failed: %s\n", strerror(errno));
        fprintf(stderr, "\n[!] Make sure AuroraOS is running in QEMU with port forwarding enabled!\n");
        fprintf(stderr, "    In another terminal run:\n");
        fprintf(stderr, "        make run        (GUI mode) or\n");
        fprintf(stderr, "        make run-term   (Console mode)\n\n");
        return 1;
    }

    /* Save original terminal settings and set raw mode */
    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
            atexit(reset_terminal);
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(ICANON | ECHO | ISIG);
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            raw_mode_set = 1;
        }
    }

    fd_set fds;
    char buf[1024];

    while (1) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(sock, &fds);
        int maxfd = (sock > STDIN_FILENO ? sock : STDIN_FILENO);

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            /* Exit on Ctrl-] (0x1D) or Ctrl-5 */
            if (n == 1 && buf[0] == 0x1d) {
                break;
            }
            if (write(sock, buf, n) < 0) break;
        }

        if (FD_ISSET(sock, &fds)) {
            ssize_t n = read(sock, buf, sizeof(buf));
            if (n <= 0) break;
            if (write(STDOUT_FILENO, buf, n) < 0) break;
        }
    }

    reset_terminal();
    close(sock);
    printf("\n[Connection to AuroraOS closed]\n");
    return 0;
}
