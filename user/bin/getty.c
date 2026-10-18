/*
 * AuroraOS /bin/getty - TTY line initialization and login prompt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

int main(int argc, char **argv)
{
    const char *tty_dev = "/dev/console";
    if (argc > 1) {
        tty_dev = argv[1];
    }

    int fd = open(tty_dev, O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    }

    /* Become session leader and claim controlling terminal */
    setsid();
    ioctl(STDIN_FILENO, TIOCSCTTY, 1);

    /* Configure termios for standard canonical interactive editing */
    struct termios tio;
    if (tcgetattr(STDIN_FILENO, &tio) == 0) {
        tio.c_iflag |= (ICRNL | IXON);
        tio.c_oflag |= (OPOST | ONLCR);
        tio.c_cflag |= (CS8 | CREAD);
        tio.c_lflag |= (ISIG | ICANON | ECHO | ECHOE | ECHOK);
        tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    }

    /* Print issue banner */
    FILE *issue_file = fopen("/etc/issue", "r");
    if (issue_file) {
        char buf[256];
        while (fgets(buf, sizeof(buf), issue_file)) {
            fputs(buf, stdout);
        }
        fclose(issue_file);
    } else {
        printf("\nAuroraOS 1.0.0 (x86_64) %s\n\n", tty_dev);
    }
    fflush(stdout);

    /* Prompt for user and execute /bin/login */
    char user[128];
    while (1) {
        printf("aurora login: ");
        fflush(stdout);

        if (!fgets(user, sizeof(user), stdin)) {
            sleep(1);
            continue;
        }

        /* Trim newline and trailing spaces */
        size_t len = strlen(user);
        while (len > 0 && (user[len - 1] == '\n' || user[len - 1] == '\r' || user[len - 1] == ' ')) {
            user[--len] = '\0';
        }

        if (len == 0) continue;

        char *login_argv[] = { "/bin/login", user, NULL };
        execve("/bin/login", login_argv, environ);

        fprintf(stderr, "getty: execve /bin/login failed: %s\n", strerror(errno));
        sleep(2);
    }

    return 0;
}
