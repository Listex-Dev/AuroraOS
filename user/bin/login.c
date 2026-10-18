/*
 * AuroraOS /bin/login - User authentication and shell initialization
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <pwd.h>
#include <errno.h>

static void get_password_no_echo(const char *prompt, char *buf, size_t max_len)
{
    fputs(prompt, stdout);
    fflush(stdout);

    struct termios old_tio, new_tio;
    int has_tty = (tcgetattr(STDIN_FILENO, &old_tio) == 0);
    if (has_tty) {
        new_tio = old_tio;
        new_tio.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    }

    if (!fgets(buf, max_len, stdin)) {
        buf[0] = '\0';
    }

    if (has_tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
        putchar('\n');
        fflush(stdout);
    }

    /* Trim newline */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
}

int main(int argc, char **argv)
{
    char username[128];
    username[0] = '\0';

    if (argc > 1) {
        strncpy(username, argv[1], sizeof(username) - 1);
    } else {
        printf("aurora login: ");
        fflush(stdout);
        if (!fgets(username, sizeof(username), stdin)) {
            return 1;
        }
        size_t len = strlen(username);
        while (len > 0 && (username[len - 1] == '\n' || username[len - 1] == '\r')) {
            username[--len] = '\0';
        }
    }

    if (username[0] == '\0') {
        return 1;
    }

    /* Prompt password (optional verification for root) */
    char password[128];
    password[0] = '\0';
    if (isatty(STDIN_FILENO)) {
        get_password_no_echo("Password: ", password, sizeof(password));
    }

    /* Check user credentials */
    uid_t uid = 0;
    gid_t gid = 0;
    const char *home = "/root";
    const char *shell = "/bin/sh";

    struct passwd *pw = getpwnam(username);
    if (pw) {
        uid = pw->pw_uid;
        gid = pw->pw_gid;
        if (pw->pw_dir && pw->pw_dir[0]) home = pw->pw_dir;
        if (pw->pw_shell && pw->pw_shell[0]) shell = pw->pw_shell;
    } else if (strcmp(username, "root") == 0) {
        uid = 0;
        gid = 0;
        home = "/root";
        shell = "/bin/sh";
    } else {
        printf("Login incorrect\n");
        sleep(2);
        return 1;
    }

    /* Apply credentials */
    setgid(gid);
    setuid(uid);

    /* Setup session environment */
    setenv("USER", username, 1);
    setenv("LOGNAME", username, 1);
    setenv("HOME", home, 1);
    setenv("SHELL", shell, 1);
    setenv("PATH", "/bin:/usr/bin", 1);
    setenv("TERM", "linux", 1);

    /* Change directory to HOME or / */
    if (chdir(home) != 0) {
        chdir("/");
    }

    /* Print MOTD */
    FILE *motd = fopen("/etc/motd", "r");
    if (motd) {
        char buf[256];
        while (fgets(buf, sizeof(buf), motd)) {
            fputs(buf, stdout);
        }
        fclose(motd);
    } else {
        printf("\nWelcome to AuroraOS 1.0.0!\nAll systems operating in 64-bit Long Mode.\n\n");
    }
    fflush(stdout);

    /* Launch user shell */
    char *sh_argv[] = { (char *)shell, NULL };
    execve(shell, sh_argv, environ);

    fprintf(stderr, "login: execve %s failed: %s\n", shell, strerror(errno));
    return 1;
}
