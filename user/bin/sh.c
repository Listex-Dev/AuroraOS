/*
 * AuroraOS /bin/sh - POSIX-compatible shell
 *
 * Supports:
 * - Builtins: cd, pwd, echo, export, unset, set, read, exit, test, [, true, false, :, jobs, fg, bg, wait, kill, type, which, clear, help, umask
 * - Pipelines: cmd1 | cmd2 | cmd3 ...
 * - Redirections: >, >>, <, 2>, 2>&1
 * - Control operators: ;, &&, ||
 * - Background execution: cmd &
 * - Job control: jobs, fg, bg, SIGTSTP / SIGCONT
 * - Variable expansion: $VAR, ${VAR}, $?, $$, $!, $0
 * - Quotes: 'single', "double", \ escaping
 * - Non-interactive (-c "cmd") and interactive prompt mode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>

#define MAX_LINE 2048
#define MAX_ARGS 128
#define MAX_CMDS 32
#define MAX_JOBS 32

typedef struct {
    int id;
    pid_t pid;
    pid_t pgid;
    int state; /* 0: empty, 1: running, 2: stopped, 3: done */
    char cmdline[128];
} job_t;

static job_t g_jobs[MAX_JOBS];
static int g_last_exit_status = 0;
static pid_t g_last_bg_pid = 0;
static pid_t g_shell_pgid = 0;
static int g_interactive = 0;
static int g_script_argc = 0;
static char **g_script_argv = NULL;

#define MAX_HISTORY 64
static char g_history[MAX_HISTORY][MAX_LINE];
static int g_hist_count = 0;

static void history_add(const char *cmd)
{
    if (!cmd || !*cmd) return;
    if (g_hist_count > 0 && strcmp(g_history[(g_hist_count - 1) % MAX_HISTORY], cmd) == 0) {
        return;
    }
    if (g_hist_count < MAX_HISTORY) {
        strncpy(g_history[g_hist_count], cmd, MAX_LINE - 1);
        g_history[g_hist_count][MAX_LINE - 1] = '\0';
        g_hist_count++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(g_history[i], g_history[i + 1]);
        }
        strncpy(g_history[MAX_HISTORY - 1], cmd, MAX_LINE - 1);
        g_history[MAX_HISTORY - 1][MAX_LINE - 1] = '\0';
    }
}

static void jobs_init(void)
{
    memset(g_jobs, 0, sizeof(g_jobs));
}

static int jobs_add(pid_t pid, pid_t pgid, int state, const char *cmd)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state == 0) {
            g_jobs[i].id = i + 1;
            g_jobs[i].pid = pid;
            g_jobs[i].pgid = pgid;
            g_jobs[i].state = state;
            strncpy(g_jobs[i].cmdline, cmd, sizeof(g_jobs[i].cmdline) - 1);
            return g_jobs[i].id;
        }
    }
    return -1;
}

static job_t *jobs_find(int id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state != 0 && g_jobs[i].id == id) {
            return &g_jobs[i];
        }
    }
    return NULL;
}

static void jobs_check_status(void)
{
    int status = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state != 0) {
            pid_t p = waitpid(g_jobs[i].pid, &status, WNOHANG | WUNTRACED);
            if (p > 0) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    if (g_interactive) {
                        printf("[%d]+  Done                    %s\n", g_jobs[i].id, g_jobs[i].cmdline);
                    }
                    g_jobs[i].state = 0;
                } else if (WIFSTOPPED(status)) {
                    g_jobs[i].state = 2;
                    if (g_interactive) {
                        printf("[%d]+  Stopped                 %s\n", g_jobs[i].id, g_jobs[i].cmdline);
                    }
                }
            }
        }
    }
}

/* Variable expansion: $VAR, ${VAR}, $?, $$, $!, $0 */
static char *expand_variables(const char *in)
{
    char *out = malloc(MAX_LINE * 2);
    if (!out) return NULL;
    size_t o = 0;
    size_t i = 0;

    while (in[i] && o < (MAX_LINE * 2 - 64)) {
        if (in[i] == '$') {
            i++;
            if (in[i] == '?') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", g_last_exit_status);
                for (size_t b = 0; buf[b]; b++) out[o++] = buf[b];
                i++;
            } else if (in[i] == '$') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", getpid());
                for (size_t b = 0; buf[b]; b++) out[o++] = buf[b];
                i++;
            } else if (in[i] == '!') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", g_last_bg_pid);
                for (size_t b = 0; buf[b]; b++) out[o++] = buf[b];
                i++;
            } else if (in[i] == '0') {
                const char *s = (g_script_argv && g_script_argc > 0) ? g_script_argv[0] : "sh";
                while (*s) out[o++] = *s++;
                i++;
            } else if (in[i] >= '1' && in[i] <= '9') {
                int idx = in[i] - '0';
                if (g_script_argv && idx < g_script_argc) {
                    const char *val = g_script_argv[idx];
                    while (*val) out[o++] = *val++;
                }
                i++;
            } else if (in[i] == '#') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", (g_script_argc > 1) ? (g_script_argc - 1) : 0);
                for (size_t b = 0; buf[b]; b++) out[o++] = buf[b];
                i++;
            } else if (in[i] == '*' || in[i] == '@') {
                if (g_script_argv) {
                    for (int k = 1; k < g_script_argc; k++) {
                        const char *val = g_script_argv[k];
                        while (*val) out[o++] = *val++;
                        if (k < g_script_argc - 1) out[o++] = ' ';
                    }
                }
            } else if (in[i] == '(') {
                i++;
                char cmd[256];
                size_t c = 0;
                int depth = 1;
                while (in[i] && c < sizeof(cmd) - 1) {
                    if (in[i] == '(') depth++;
                    else if (in[i] == ')') {
                        depth--;
                        if (depth == 0) break;
                    }
                    cmd[c++] = in[i++];
                }
                cmd[c] = '\0';
                if (in[i] == ')') i++;

                int pfd[2];
                if (pipe(pfd) == 0) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        close(pfd[0]);
                        dup2(pfd[1], STDOUT_FILENO);
                        close(pfd[1]);
                        int devnull = open("/dev/null", O_RDONLY);
                        if (devnull >= 0) {
                            dup2(devnull, STDIN_FILENO);
                            close(devnull);
                        }
                        char *sh_argv[] = { "/bin/sh", "-c", cmd, NULL };
                        execve("/bin/sh", sh_argv, environ);
                        _exit(1);
                    }
                    close(pfd[1]);
                    char res_buf[512];
                    ssize_t n = read(pfd[0], res_buf, sizeof(res_buf) - 1);
                    close(pfd[0]);
                    waitpid(pid, NULL, 0);
                    if (n > 0) {
                        while (n > 0 && (res_buf[n - 1] == '\n' || res_buf[n - 1] == '\r' || res_buf[n - 1] == ' ')) {
                            n--;
                        }
                        for (ssize_t k = 0; k < n; k++) {
                            out[o++] = res_buf[k];
                        }
                    }
                }
            } else if (in[i] == '{') {
                i++;
                char var[128];
                size_t v = 0;
                while (in[i] && in[i] != '}' && v < sizeof(var) - 1) {
                    var[v++] = in[i++];
                }
                var[v] = '\0';
                if (in[i] == '}') i++;
                const char *val = getenv(var);
                if (val) {
                    while (*val) out[o++] = *val++;
                }
            } else if (isalpha((unsigned char)in[i]) || in[i] == '_') {
                char var[128];
                size_t v = 0;
                while ((isalnum((unsigned char)in[i]) || in[i] == '_') && v < sizeof(var) - 1) {
                    var[v++] = in[i++];
                }
                var[v] = '\0';
                const char *val = getenv(var);
                if (val) {
                    while (*val) out[o++] = *val++;
                }
            } else {
                out[o++] = '$';
            }
        } else {
            out[o++] = in[i++];
        }
    }
    out[o] = '\0';
    return out;
}

/* Parse command into argv array respecting single and double quotes */
static int tokenize_args(char *str, char **argv, int max_args)
{
    int argc = 0;
    char *p = str;

    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '\0' || *p == '#') break;

        char *token = p;
        char *dst = p;
        int in_token = 0;

        while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
            in_token = 1;
            if (*p == '\'') {
                p++;
                while (*p && *p != '\'') {
                    *dst++ = *p++;
                }
                if (*p == '\'') p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && p[1]) p++;
                    *dst++ = *p++;
                }
                if (*p == '"') p++;
            } else if (*p == '\\' && p[1]) {
                p++;
                *dst++ = *p++;
            } else {
                *dst++ = *p++;
            }
        }

        char delim = *p;
        *dst = '\0';
        if (in_token || dst > token) {
            argv[argc++] = token;
        }
        if (delim != '\0') {
            p++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

/* Builtin implementations */
static int builtin_cd(int argc, char **argv)
{
    const char *target = NULL;
    if (argc < 2) {
        target = getenv("HOME");
        if (!target) target = "/";
    } else {
        target = argv[1];
    }
    if (chdir(target) != 0) {
        fprintf(stderr, "sh: cd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    return 0;
}

static int builtin_pwd(void)
{
    char buf[1024];
    if (getcwd(buf, sizeof(buf))) {
        printf("%s\n", buf);
        return 0;
    }
    return 1;
}

static int builtin_echo(int argc, char **argv)
{
    int newline = 1;
    int start = 1;
    if (argc > 1 && argv[1] && strcmp(argv[1], "-n") == 0) {
        newline = 0;
        start = 2;
    }
    for (int i = start; i < argc; i++) {
        if (argv[i]) {
            fputs(argv[i], stdout);
            if (i < argc - 1) putchar(' ');
        }
    }
    if (newline) putchar('\n');
    fflush(stdout);
    return 0;
}

static int builtin_sleep(int argc, char **argv)
{
    if (argc < 2) return 1;
    int s = atoi(argv[1]);
    if (s > 0) sleep(s);
    return 0;
}

static int builtin_export(int argc, char **argv)
{
    if (argc < 2) {
        for (char **e = environ; *e; e++) {
            printf("export %s\n", *e);
        }
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            setenv(argv[i], eq + 1, 1);
            *eq = '=';
        } else {
            setenv(argv[i], "", 0);
        }
    }
    return 0;
}

static int builtin_unset(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        unsetenv(argv[i]);
    }
    return 0;
}

static int builtin_read(int argc, char **argv)
{
    if (argc < 2) return 1;
    char line[512];
    if (!fgets(line, sizeof(line), stdin)) return 1;
    size_t l = strlen(line);
    while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r')) {
        line[--l] = '\0';
    }
    setenv(argv[1], line, 1);
    return 0;
}

static int builtin_test(int argc, char **argv)
{
    if (argc <= 1) return 1;
    if (strcmp(argv[argc - 1], "]") == 0) argc--;

    if (argc == 2) {
        return (strlen(argv[1]) > 0) ? 0 : 1;
    }
    if (argc == 3) {
        if (strcmp(argv[1], "-z") == 0) {
            return (strlen(argv[2]) == 0) ? 0 : 1;
        }
        if (strcmp(argv[1], "-n") == 0) {
            return (strlen(argv[2]) > 0) ? 0 : 1;
        }
        if (strcmp(argv[1], "-f") == 0) {
            struct stat st;
            return (stat(argv[2], &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
        }
        if (strcmp(argv[1], "-d") == 0) {
            struct stat st;
            return (stat(argv[2], &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
        }
        if (strcmp(argv[1], "-e") == 0) {
            struct stat st;
            return (stat(argv[2], &st) == 0) ? 0 : 1;
        }
    }
    if (argc == 4) {
        if (strcmp(argv[2], "=") == 0 || strcmp(argv[2], "==") == 0) {
            return (strcmp(argv[1], argv[3]) == 0) ? 0 : 1;
        }
        if (strcmp(argv[2], "!=") == 0) {
            return (strcmp(argv[1], argv[3]) != 0) ? 0 : 1;
        }
        if (strcmp(argv[2], "-eq") == 0) {
            return (atoi(argv[1]) == atoi(argv[3])) ? 0 : 1;
        }
        if (strcmp(argv[2], "-ne") == 0) {
            return (atoi(argv[1]) != atoi(argv[3])) ? 0 : 1;
        }
        if (strcmp(argv[2], "-lt") == 0) {
            return (atoi(argv[1]) < atoi(argv[3])) ? 0 : 1;
        }
        if (strcmp(argv[2], "-gt") == 0) {
            return (atoi(argv[1]) > atoi(argv[3])) ? 0 : 1;
        }
        if (strcmp(argv[2], "-le") == 0) {
            return (atoi(argv[1]) <= atoi(argv[3])) ? 0 : 1;
        }
        if (strcmp(argv[2], "-ge") == 0) {
            return (atoi(argv[1]) >= atoi(argv[3])) ? 0 : 1;
        }
    }
    return 1;
}

static int builtin_jobs(void)
{
    jobs_check_status();
    for (int i = 0; i < MAX_JOBS; i++) {
        if (g_jobs[i].state != 0) {
            const char *st_str = (g_jobs[i].state == 1) ? "Running" : "Stopped";
            printf("[%d]+  %-24s%s\n", g_jobs[i].id, st_str, g_jobs[i].cmdline);
        }
    }
    return 0;
}

static int builtin_fg(int argc, char **argv)
{
    int jid = 1;
    if (argc > 1) {
        const char *arg = argv[1];
        if (*arg == '%') arg++;
        jid = atoi(arg);
    }
    job_t *j = jobs_find(jid);
    if (!j) {
        fprintf(stderr, "sh: fg: %d: no such job\n", jid);
        return 1;
    }
    printf("%s\n", j->cmdline);
    tcsetpgrp(STDIN_FILENO, j->pgid ? j->pgid : j->pid);
    if (j->state == 2) {
        kill(j->pid, SIGCONT);
    }
    j->state = 1;
    int status = 0;
    waitpid(j->pid, &status, WUNTRACED);
    tcsetpgrp(STDIN_FILENO, g_shell_pgid);
    if (WIFSTOPPED(status)) {
        j->state = 2;
        printf("\n[%d]+  Stopped                 %s\n", j->id, j->cmdline);
    } else {
        j->state = 0;
    }
    return WEXITSTATUS(status);
}

static int builtin_bg(int argc, char **argv)
{
    int jid = 1;
    if (argc > 1) {
        const char *arg = argv[1];
        if (*arg == '%') arg++;
        jid = atoi(arg);
    }
    job_t *j = jobs_find(jid);
    if (!j) {
        fprintf(stderr, "sh: bg: %d: no such job\n", jid);
        return 1;
    }
    kill(j->pid, SIGCONT);
    j->state = 1;
    printf("[%d]+ %s &\n", j->id, j->cmdline);
    return 0;
}

static int builtin_kill(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "sh: kill: usage: kill [-sig] pid\n");
        return 1;
    }
    int sig = SIGTERM;
    int pid_arg = 1;
    if (argv[1][0] == '-') {
        if (strcmp(&argv[1][1], "0") == 0) {
            sig = 0;
        } else {
            sig = atoi(&argv[1][1]);
            if (sig <= 0) sig = SIGTERM;
        }
        pid_arg = 2;
    }
    if (pid_arg >= argc) return 1;
    pid_t target = atoi(argv[pid_arg]);
    if (kill(target, sig) != 0) {
        perror("sh: kill");
        return 1;
    }
    return 0;
}

static int builtin_killall(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "sh: killall: usage: killall [-sig] name\n");
        return 1;
    }
    int sig = SIGTERM;
    int name_arg = 1;
    if (argv[1][0] == '-') {
        if (strcmp(&argv[1][1], "9") == 0) sig = SIGKILL;
        else sig = atoi(&argv[1][1]);
        if (sig <= 0) sig = SIGTERM;
        name_arg = 2;
    }
    if (name_arg >= argc) return 1;
    const char *target = argv[name_arg];

    DIR *d = opendir("/proc");
    if (!d) return 1;
    struct dirent *ent;
    int count = 0;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] >= '0' && ent->d_name[0] <= '9') {
            pid_t pid = atoi(ent->d_name);
            if (pid <= 1 || pid == getpid()) continue;
            char path[256];
            snprintf(path, sizeof(path), "/proc/%s/status", ent->d_name);
            FILE *f = fopen(path, "r");
            if (f) {
                char line[128];
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, "Name:", 5) == 0) {
                        char *p = line + 5;
                        while (*p == ' ' || *p == '\t') p++;
                        size_t l = strlen(p);
                        while (l > 0 && (p[l - 1] == '\n' || p[l - 1] == '\r')) p[--l] = '\0';
                        if (strcmp(p, target) == 0) {
                            kill(pid, sig);
                            count++;
                        }
                        break;
                    }
                }
                fclose(f);
            }
        }
    }
    closedir(d);
    return count > 0 ? 0 : 1;
}

static int builtin_rm(int argc, char **argv)
{
    if (argc < 2) return 1;
    int force = 0;
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "-rf") == 0) {
            force = 1;
            continue;
        }
        if (unlink(argv[i]) != 0) {
            if (!force) status = 1;
        }
    }
    return status;
}

static int builtin_type(int argc, char **argv)
{
    if (argc < 2) return 1;
    const char *builtins[] = {
        "cd", "pwd", "echo", "export", "unset", "set", "read", "exit",
        "test", "[", "true", "false", ":", "jobs", "fg", "bg", "wait",
        "kill", "type", "which", "clear", "help", "umask", "sleep", NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(argv[1], builtins[i]) == 0) {
            printf("%s is a shell builtin\n", argv[1]);
            return 0;
        }
    }
    printf("%s is /bin/%s\n", argv[1], argv[1]);
    return 0;
}

static int check_builtin(int argc, char **argv, int *ret)
{
    if (argc == 0) return 0;

    if (strcmp(argv[0], "cd") == 0) {
        *ret = builtin_cd(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        *ret = builtin_pwd();
        return 1;
    }
    if (strcmp(argv[0], "echo") == 0) {
        *ret = builtin_echo(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "sleep") == 0) {
        *ret = builtin_sleep(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "export") == 0) {
        *ret = builtin_export(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "unset") == 0) {
        *ret = builtin_unset(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "set") == 0) {
        for (char **e = environ; *e; e++) {
            printf("%s\n", *e);
        }
        *ret = 0;
        return 1;
    }
    if (strcmp(argv[0], "test") == 0 || strcmp(argv[0], "[") == 0) {
        *ret = builtin_test(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "true") == 0 || strcmp(argv[0], ":") == 0) {
        *ret = 0;
        return 1;
    }
    if (strcmp(argv[0], "false") == 0) {
        *ret = 1;
        return 1;
    }
    if (strcmp(argv[0], "jobs") == 0) {
        *ret = builtin_jobs();
        return 1;
    }
    if (strcmp(argv[0], "fg") == 0) {
        *ret = builtin_fg(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "bg") == 0) {
        *ret = builtin_bg(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "kill") == 0) {
        *ret = builtin_kill(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "killall") == 0) {
        *ret = builtin_killall(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "rm") == 0) {
        *ret = builtin_rm(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "type") == 0 || strcmp(argv[0], "which") == 0) {
        *ret = builtin_type(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "clear") == 0) {
        printf("\033[2J\033[H");
        fflush(stdout);
        *ret = 0;
        return 1;
    }
    if (strcmp(argv[0], "history") == 0) {
        for (int i = 0; i < g_hist_count; i++) {
            printf("%4d  %s\n", i + 1, g_history[i]);
        }
        *ret = 0;
        return 1;
    }
    if (strcmp(argv[0], "read") == 0) {
        *ret = builtin_read(argc, argv);
        return 1;
    }
    if (strcmp(argv[0], "exit") == 0) {
        int code = (argc > 1) ? atoi(argv[1]) : g_last_exit_status;
        exit(code);
    }
    if (strcmp(argv[0], "help") == 0) {
        printf("AuroraOS POSIX Shell (/bin/sh)\n");
        printf("Builtin commands: cd, pwd, echo, export, unset, set, test, [, true, false, :, jobs, fg, bg, wait, kill, type, which, clear, history, help, exit\n");
        *ret = 0;
        return 1;
    }
    return 0;
}

/* Parse redirections out of an argument list */
static int extract_redirections(char **argv, char **in_file, char **out_file, int *append, char **err_file, int *err_to_out)
{
    *in_file = NULL;
    *out_file = NULL;
    *append = 0;
    *err_file = NULL;
    *err_to_out = 0;

    int r = 0;
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "<") == 0) {
            if (argv[i + 1]) {
                *in_file = argv[i + 1];
                i++;
            }
        } else if (strncmp(argv[i], "<", 1) == 0 && argv[i][1] != '\0') {
            *in_file = &argv[i][1];
        } else if (strcmp(argv[i], ">>") == 0) {
            if (argv[i + 1]) {
                *out_file = argv[i + 1];
                *append = 1;
                i++;
            }
        } else if (strncmp(argv[i], ">>", 2) == 0 && argv[i][2] != '\0') {
            *out_file = &argv[i][2];
            *append = 1;
        } else if (strcmp(argv[i], "2>&1") == 0) {
            *err_to_out = 1;
        } else if (strncmp(argv[i], "2>", 2) == 0) {
            if (argv[i][2] != '\0') {
                *err_file = &argv[i][2];
            } else if (argv[i + 1]) {
                *err_file = argv[i + 1];
                i++;
            }
        } else if (strcmp(argv[i], ">") == 0) {
            if (argv[i + 1]) {
                *out_file = argv[i + 1];
                *append = 0;
                i++;
            }
        } else if (strncmp(argv[i], ">", 1) == 0 && argv[i][1] != '\0') {
            *out_file = &argv[i][1];
            *append = 0;
        } else {
            argv[r++] = argv[i];
        }
    }
    argv[r] = NULL;
    return r;
}

/* Execute a single simple command (or pipeline segment) */
static int execute_command(char *cmd_str, int in_fd, int out_fd, pid_t *child_pid, pid_t pgid, const int *pipe_fds, int num_pipes, int is_bg)
{
    char *argv[MAX_ARGS];
    int argc = tokenize_args(cmd_str, argv, MAX_ARGS);
    if (argc == 0) return 0;

    char *in_file = NULL;
    char *out_file = NULL;
    char *err_file = NULL;
    int append = 0;
    int err_to_out = 0;
    argc = extract_redirections(argv, &in_file, &out_file, &append, &err_file, &err_to_out);

    if (argc == 0 || argv[0] == NULL) return 0;

    /* Check for standalone variable assignment: VAR=val */
    if (argc == 1 && strchr(argv[0], '=') != NULL && argv[0][0] != '=') {
        char *eq = strchr(argv[0], '=');
        *eq = '\0';
        setenv(argv[0], eq + 1, 1);
        *eq = '=';
        if (child_pid) *child_pid = 0;
        return 0;
    }

    /* Check if builtin should be run in current process (only if not background and not in a pipe/redirection) */
    if (!is_bg && in_fd == STDIN_FILENO && out_fd == STDOUT_FILENO && !in_file && !out_file && !err_file && !err_to_out) {
        int ret = 0;
        if (check_builtin(argc, argv, &ret)) {
            g_last_exit_status = ret;
            if (child_pid) *child_pid = 0;
            return ret;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("sh: fork");
        return -1;
    }

    if (pid == 0) {
        /* Child */
        if (pgid == 0) setpgid(0, 0);
        else setpgid(0, pgid);

        /* Connect pipeline fds */
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
        }
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
        }

        /* Close all pipe fds inherited from pipeline in child */
        for (int k = 0; k < num_pipes; k++) {
            close(pipe_fds[k]);
        }

        /* File redirections */
        if (in_file) {
            int fd = open(in_file, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "sh: %s: %s\n", in_file, strerror(errno));
                _exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (out_file) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd = open(out_file, flags, 0666);
            if (fd < 0) {
                fprintf(stderr, "sh: %s: %s\n", out_file, strerror(errno));
                _exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (err_file) {
            int fd = open(err_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                fprintf(stderr, "sh: %s: %s\n", err_file, strerror(errno));
                _exit(1);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        if (err_to_out) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
        }

        /* Check builtin in child */
        int ret = 0;
        if (check_builtin(argc, argv, &ret)) {
            _exit(ret);
        }

        execvp(argv[0], argv);
        fprintf(stderr, "sh: %s: command not found\n", argv[0]);
        _exit(127);
    }

    /* Parent */
    if (child_pid) *child_pid = pid;
    if (pgid == 0) setpgid(pid, pid);
    else setpgid(pid, pgid);

    return 0;
}

/* Execute a pipeline: cmd1 | cmd2 | cmd3 ... */
static int execute_pipeline(char *pipe_str, int is_bg)
{
    char *cmds[MAX_CMDS];
    int num_cmds = 0;

    char *p = pipe_str;
    char in_q = 0;
    while (*p) {
        while (*p == ' ') p++;
        char *start = p;
        while (*p) {
            if (*p == '\'' || *p == '"') {
                if (!in_q) in_q = *p;
                else if (in_q == *p) in_q = 0;
            } else if (!in_q && *p == '|') {
                break;
            }
            p++;
        }
        if (*p == '|') {
            *p = '\0';
            cmds[num_cmds++] = start;
            p++;
        } else {
            cmds[num_cmds++] = start;
            break;
        }
    }

    if (num_cmds == 0) return 0;

    int pipefds[2 * (MAX_CMDS - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("sh: pipe");
            return 1;
        }
    }

    pid_t pids[MAX_CMDS];
    pid_t leader_pgid = 0;

    for (int i = 0; i < num_cmds; i++) {
        int in_fd = (i == 0) ? STDIN_FILENO : pipefds[(i - 1) * 2];
        int out_fd = (i == num_cmds - 1) ? STDOUT_FILENO : pipefds[i * 2 + 1];

        execute_command(cmds[i], in_fd, out_fd, &pids[i], leader_pgid, pipefds, 2 * (num_cmds - 1), is_bg);
        if (i == 0 && pids[0] > 0) {
            leader_pgid = pids[0];
        }
    }

    /* Close all pipe fds in parent */
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }

    if (is_bg) {
        int jid = jobs_add(leader_pgid, leader_pgid, 1, pipe_str);
        g_last_bg_pid = leader_pgid;
        printf("[%d] %d\n", jid, leader_pgid);
        return 0;
    }

    /* Wait for foreground pipeline */
    int status = 0;
    for (int i = 0; i < num_cmds; i++) {
        if (pids[i] > 0) {
            waitpid(pids[i], &status, 0);
            if (i == num_cmds - 1) {
                g_last_exit_status = WEXITSTATUS(status);
            }
        }
    }

    return g_last_exit_status;
}

/* Parse sequences: ;, &&, || and background & */
static int execute_line(char *line)
{
    char *p = line;
    int last_status = 0;

    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '\0' || *p == '#') break;

        char *cmd_start = p;
        int sep_type = 0; /* 0: none, 1: ';', 2: '&&', 3: '||', 4: '&' */
        char in_q = 0;

        while (*p) {
            if (*p == '\'' || *p == '"') {
                if (in_q == 0) in_q = *p;
                else if (in_q == *p) in_q = 0;
            }
            if (!in_q) {
                if (*p == ';') {
                    *p = '\0';
                    p++;
                    sep_type = 1;
                    break;
                } else if (*p == '&' && p[1] == '&') {
                    *p = '\0';
                    p += 2;
                    sep_type = 2;
                    break;
                } else if (*p == '|' && p[1] == '|') {
                    *p = '\0';
                    p += 2;
                    sep_type = 3;
                    break;
                } else if (*p == '&') {
                    if (p > cmd_start && (*(p - 1) == '>' || *(p - 1) == '<')) {
                        /* Part of redirection >& or <& */
                    } else {
                        *p = '\0';
                        p++;
                        sep_type = 4;
                        break;
                    }
                }
            }
            p++;
        }

        char *expanded_cmd = expand_variables(cmd_start);
        int is_bg = (sep_type == 4);
        last_status = execute_pipeline(expanded_cmd ? expanded_cmd : cmd_start, is_bg);
        g_last_exit_status = last_status;
        if (expanded_cmd) free(expanded_cmd);

        if (sep_type == 2 && last_status != 0) {
            /* '&&' condition failed: skip until next '||', ';', or end */
            int skip_sep = 0;
            while (*p) {
                while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                if (*p == '\0' || *p == '#') { p += strlen(p); break; }
                char q = 0;
                while (*p) {
                    if (*p == '\'' || *p == '"') {
                        if (!q) q = *p;
                        else if (q == *p) q = 0;
                    }
                    if (!q) {
                        if (*p == ';') { *p = '\0'; p++; skip_sep = 1; break; }
                        if (*p == '&' && p[1] == '&') { *p = '\0'; p += 2; break; }
                        if (*p == '|' && p[1] == '|') { *p = '\0'; p += 2; skip_sep = 3; break; }
                        if (*p == '&') {
                            if (p > cmd_start && (*(p - 1) == '>' || *(p - 1) == '<')) {
                                /* Part of redirection */
                            } else {
                                *p = '\0'; p++; skip_sep = 4; break;
                            }
                        }
                    }
                    p++;
                }
                if (skip_sep == 1 || skip_sep == 3 || skip_sep == 4 || !*p) break;
            }
            continue;
        }
        if (sep_type == 3 && last_status == 0) {
            /* '||' condition succeeded: skip until next '&&', ';', or end */
            int skip_sep = 0;
            while (*p) {
                while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                if (*p == '\0' || *p == '#') { p += strlen(p); break; }
                char q = 0;
                while (*p) {
                    if (*p == '\'' || *p == '"') {
                        if (!q) q = *p;
                        else if (q == *p) q = 0;
                    }
                    if (!q) {
                        if (*p == ';') { *p = '\0'; p++; skip_sep = 1; break; }
                        if (*p == '|' && p[1] == '|') { *p = '\0'; p += 2; break; }
                        if (*p == '&' && p[1] == '&') { *p = '\0'; p += 2; skip_sep = 2; break; }
                        if (*p == '&') {
                            if (p > cmd_start && (*(p - 1) == '>' || *(p - 1) == '<')) {
                                /* Part of redirection */
                            } else {
                                *p = '\0'; p++; skip_sep = 4; break;
                            }
                        }
                    }
                    p++;
                }
                if (skip_sep == 1 || skip_sep == 2 || skip_sep == 4 || !*p) break;
            }
            continue;
        }
    }

    return last_status;
}

static void redraw_line(const char *prompt, const char *buf, size_t old_len, size_t new_len)
{
    (void)old_len;
    write(STDOUT_FILENO, "\r\033[2K", 5);
    write(STDOUT_FILENO, prompt, strlen(prompt));
    if (new_len > 0) {
        write(STDOUT_FILENO, buf, new_len);
    }
}

static char *sh_readline(const char *prompt, char *buf, size_t max_len)
{
    if (!g_interactive) {
        if (!fgets(buf, max_len, stdin)) return NULL;
        size_t l = strlen(buf);
        if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
        return buf;
    }

    struct termios orig_tio, raw_tio;
    if (tcgetattr(STDIN_FILENO, &orig_tio) != 0) {
        fputs(prompt, stdout);
        fflush(stdout);
        if (!fgets(buf, max_len, stdin)) return NULL;
        size_t l = strlen(buf);
        if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
        return buf;
    }

    fputs(prompt, stdout);
    fflush(stdout);

    raw_tio = orig_tio;
    raw_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_tio);

    size_t len = 0;
    size_t pos = 0;
    buf[0] = '\0';

    char current_saved[MAX_LINE];
    current_saved[0] = '\0';
    int hist_idx = g_hist_count;

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
            if (len == 0) return NULL;
            break;
        }

        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }

        if (c == 0x03) { /* Ctrl-C */
            write(STDOUT_FILENO, "^C\n", 3);
            buf[0] = '\0';
            len = 0;
            pos = 0;
            hist_idx = g_hist_count;
            current_saved[0] = '\0';
            fputs(prompt, stdout);
            fflush(stdout);
            continue;
        }

        if (c == 0x04) { /* Ctrl-D */
            if (len == 0) {
                tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
                return NULL;
            }
            if (pos < len) {
                memmove(&buf[pos], &buf[pos + 1], len - pos);
                len--;
                buf[len] = '\0';
                write(STDOUT_FILENO, &buf[pos], len - pos);
                write(STDOUT_FILENO, " \b", 2);
                for (size_t k = 0; k < len - pos; k++) {
                    write(STDOUT_FILENO, "\b", 1);
                }
            }
            continue;
        }

        if (c == 0x0C) { /* Ctrl-L: Clear screen */
            write(STDOUT_FILENO, "\033[2J\033[H", 7);
            fputs(prompt, stdout);
            fflush(stdout);
            write(STDOUT_FILENO, buf, len);
            for (size_t k = pos; k < len; k++) {
                write(STDOUT_FILENO, "\b", 1);
            }
            continue;
        }

        if (c == 0x01) { /* Ctrl-A: Home */
            while (pos > 0) {
                write(STDOUT_FILENO, "\b", 1);
                pos--;
            }
            continue;
        }

        if (c == 0x05) { /* Ctrl-E: End */
            while (pos < len) {
                write(STDOUT_FILENO, &buf[pos], 1);
                pos++;
            }
            continue;
        }

        if (c == 0x15) { /* Ctrl-U: Clear line */
            size_t old_len = len;
            len = 0;
            pos = 0;
            buf[0] = '\0';
            redraw_line(prompt, buf, old_len, 0);
            continue;
        }

        /* Escape sequence (arrows, function keys, delete) */
        if (c == '\033') {
            char seq[8];
            int avail = 0;
            for (int t = 0; t < 25; t++) {
                if (ioctl(STDIN_FILENO, FIONREAD, &avail) == 0 && avail > 0) break;
                usleep(1000);
            }
            if (avail > 0 && read(STDIN_FILENO, &seq[0], 1) == 1) {
                if (seq[0] == '[' || seq[0] == 'O') {
                    avail = 0;
                    for (int t = 0; t < 25; t++) {
                        if (ioctl(STDIN_FILENO, FIONREAD, &avail) == 0 && avail > 0) break;
                        usleep(1000);
                    }
                    if (avail > 0 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                        if (seq[1] == 'A') {
                            /* UP ARROW - Previous history */
                            if (hist_idx > 0) {
                                if (hist_idx == g_hist_count) {
                                    strncpy(current_saved, buf, MAX_LINE - 1);
                                    current_saved[MAX_LINE - 1] = '\0';
                                }
                                hist_idx--;
                                size_t old_len = len;
                                strncpy(buf, g_history[hist_idx], max_len - 1);
                                buf[max_len - 1] = '\0';
                                len = strlen(buf);
                                pos = len;
                                redraw_line(prompt, buf, old_len, len);
                            }
                        } else if (seq[1] == 'B') {
                            /* DOWN ARROW - Next history */
                            if (hist_idx < g_hist_count) {
                                hist_idx++;
                                size_t old_len = len;
                                if (hist_idx == g_hist_count) {
                                    strncpy(buf, current_saved, max_len - 1);
                                } else {
                                    strncpy(buf, g_history[hist_idx], max_len - 1);
                                }
                                buf[max_len - 1] = '\0';
                                len = strlen(buf);
                                pos = len;
                                redraw_line(prompt, buf, old_len, len);
                            }
                        } else if (seq[1] == 'C') {
                            /* RIGHT ARROW */
                            if (pos < len) {
                                write(STDOUT_FILENO, &buf[pos], 1);
                                pos++;
                            }
                        } else if (seq[1] == 'D') {
                            /* LEFT ARROW */
                            if (pos > 0) {
                                write(STDOUT_FILENO, "\b", 1);
                                pos--;
                            }
                        } else if (seq[1] == 'H') {
                            /* HOME */
                            while (pos > 0) {
                                write(STDOUT_FILENO, "\b", 1);
                                pos--;
                            }
                        } else if (seq[1] == 'F') {
                            /* END */
                            while (pos < len) {
                                write(STDOUT_FILENO, &buf[pos], 1);
                                pos++;
                            }
                        } else if (seq[1] >= '0' && seq[1] <= '9') {
                            char seq2[4];
                            if (read(STDIN_FILENO, &seq2[0], 1) == 1 && seq2[0] == '~') {
                                if (seq[1] == '3') {
                                    /* DELETE */
                                    if (pos < len) {
                                        memmove(&buf[pos], &buf[pos + 1], len - pos);
                                        len--;
                                        buf[len] = '\0';
                                        write(STDOUT_FILENO, &buf[pos], len - pos);
                                        write(STDOUT_FILENO, " \b", 2);
                                        for (size_t k = 0; k < len - pos; k++) {
                                            write(STDOUT_FILENO, "\b", 1);
                                        }
                                    }
                                } else if (seq[1] == '1' || seq[1] == '7') {
                                    /* HOME */
                                    while (pos > 0) {
                                        write(STDOUT_FILENO, "\b", 1);
                                        pos--;
                                    }
                                } else if (seq[1] == '4' || seq[1] == '8') {
                                    /* END */
                                    while (pos < len) {
                                        write(STDOUT_FILENO, &buf[pos], 1);
                                        pos++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            continue;
        }

        /* BACKSPACE */
        if (c == '\b' || (unsigned char)c == 0x7F || c == 0x08) {
            if (pos > 0) {
                pos--;
                memmove(&buf[pos], &buf[pos + 1], len - pos);
                len--;
                buf[len] = '\0';
                write(STDOUT_FILENO, "\b", 1);
                write(STDOUT_FILENO, &buf[pos], len - pos);
                write(STDOUT_FILENO, " \b", 2);
                for (size_t k = 0; k < len - pos; k++) {
                    write(STDOUT_FILENO, "\b", 1);
                }
            }
            continue;
        }

        /* Ignore Tab if not implementing autocomplete */
        if (c == '\t') {
            continue;
        }

        /* Printable character */
        if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
            if (len < max_len - 1) {
                if (pos < len) {
                    memmove(&buf[pos + 1], &buf[pos], len - pos);
                    buf[pos] = c;
                    len++;
                    buf[len] = '\0';
                    write(STDOUT_FILENO, &buf[pos], len - pos);
                    pos++;
                    for (size_t k = 0; k < len - pos; k++) {
                        write(STDOUT_FILENO, "\b", 1);
                    }
                } else {
                    buf[pos++] = c;
                    len++;
                    buf[len] = '\0';
                    write(STDOUT_FILENO, &c, 1);
                }
            }
            continue;
        }
    }

    buf[len] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_tio);
    if (len > 0) {
        history_add(buf);
    }
    return buf;
}

int main(int argc, char **argv)
{
    g_shell_pgid = getpgrp();
    jobs_init();

    /* Check for -c command option */
    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        return execute_line(argv[2]);
    }

    /* Check for script file argument */
    if (argc >= 2 && argv[1][0] != '-') {
        g_script_argc = argc - 1;
        g_script_argv = &argv[1];
        FILE *f = fopen(argv[1], "r");
        if (!f) {
            fprintf(stderr, "sh: %s: %s\n", argv[1], strerror(errno));
            return 1;
        }
        char line[MAX_LINE];
        int ret = 0;
        while (fgets(line, sizeof(line), f)) {
            ret = execute_line(line);
        }
        fclose(f);
        return ret;
    }

    /* Interactive mode */
    g_interactive = isatty(STDIN_FILENO);

    char line[MAX_LINE];
    while (1) {
        jobs_check_status();

        const char *ps1 = getenv("PS1");
        if (!ps1) {
            ps1 = (getuid() == 0) ? "aurora# " : "aurora$ ";
        }

        if (!sh_readline(ps1, line, sizeof(line))) {
            if (g_interactive) putchar('\n');
            break;
        }

        execute_line(line);
    }

    return g_last_exit_status;
}
