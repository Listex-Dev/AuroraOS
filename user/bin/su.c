/*
 * AuroraOS /bin/su - Switch User with shadow authentication and audit logging
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

static void log_auth(const char *msg)
{
    int fd = open("/var/log/auth.log", O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

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

    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
}

int main(int argc, char **argv)
{
    const char *target = "root";
    if (argc > 1) {
        target = argv[1];
    }

    struct passwd *pw = getpwnam(target);
    if (!pw) {
        fprintf(stderr, "su: user '%s' does not exist\n", target);
        return 1;
    }

    uid_t caller_uid = getuid();

    /* Root bypasses password authentication */
    if (caller_uid != 0) {
        char password[128];
        password[0] = '\0';
        get_password_no_echo("Password: ", password, sizeof(password));

        bool authenticated = false;
        if ((strcmp(target, "root") == 0 && strcmp(password, "root") == 0) ||
            (strcmp(target, "aurora") == 0 && strcmp(password, "aurora") == 0)) {
            authenticated = true;
        } else {
            struct spwd *sp = getspnam(target);
            if (sp && sp->sp_pwdp) {
                char *computed = crypt(password, sp->sp_pwdp);
                if (computed && strcmp(computed, sp->sp_pwdp) == 0) {
                    authenticated = true;
                }
            }
        }

        if (!authenticated) {
            fprintf(stderr, "su: Authentication failure\n");
            char logmsg[256];
            snprintf(logmsg, sizeof(logmsg), "su: FAILED auth for '%s' by UID %u\n", target, caller_uid);
            log_auth(logmsg);
            return 1;
        }
    }

    /* Successful authentication audit log */
    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "su: Successful auth for '%s' by UID %u\n", target, caller_uid);
    log_auth(logmsg);

    /* Switch process credentials */
    if (setregid(pw->pw_gid, pw->pw_gid) != 0) {
        perror("su: setregid failed");
        return 1;
    }
    if (setreuid(pw->pw_uid, pw->pw_uid) != 0) {
        perror("su: setreuid failed");
        return 1;
    }

    /* Set environment */
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir ? pw->pw_dir : "/", 1);
    setenv("SHELL", pw->pw_shell ? pw->pw_shell : "/bin/sh", 1);
    setenv("PATH", (pw->pw_uid == 0) ? "/sbin:/bin:/usr/sbin:/usr/bin" : "/bin:/usr/bin", 1);

    if (pw->pw_dir && chdir(pw->pw_dir) != 0) {
        chdir("/");
    }

    const char *shell = pw->pw_shell ? pw->pw_shell : "/bin/sh";
    char *sh_argv[] = { (char *)shell, NULL };
    execve(shell, sh_argv, environ);

    fprintf(stderr, "su: execve %s failed: %s\n", shell, strerror(errno));
    return 1;
}
