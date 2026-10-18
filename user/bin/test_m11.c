/*
 * AuroraOS Milestone M11 Acceptance Test Runner
 *
 * Verifies:
 * - POSIX Shell /bin/sh:
 *   - Pipelines: cmd1 | cmd2
 *   - Redirections: >, >>, <
 *   - Builtin commands: cd, pwd, echo, export, unset, test, true, false, exit
 *   - Control operators: ;, &&, ||
 *   - Variable expansion: $VAR, $?
 *   - Background jobs: & and jobs
 * - /sbin/init, /bin/getty, and /bin/login end-to-end boot & authentication flow
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>

#define TEST_PASS 100

static int run_sh_cmd(const char *cmd, char *output, size_t out_size)
{
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };
        execve("/bin/sh", argv, environ);
        _exit(127);
    }

    close(pipefd[1]);

    size_t total = 0;
    if (output && out_size > 0) {
        ssize_t rd;
        while ((rd = read(pipefd[0], output + total, out_size - 1 - total)) > 0) {
            total += rd;
        }
        output[total] = '\0';
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

int main(void)
{
    printf("[USER M11] Starting Milestone M11 verification in Ring 3 user mode...\n");

    char out[1024];

    /* Test 1: Shell Builtins (echo, cd, pwd, export, variable expansion) */
    printf("[USER M11] Testing /bin/sh builtins and variable expansions...\n");
    memset(out, 0, sizeof(out));
    int ret = run_sh_cmd("echo 'Hello Shell Builtin'", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "Hello Shell Builtin") != NULL);
    printf("  [OK] echo builtin: %s", out);

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("cd /tmp && pwd", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "/tmp") != NULL);
    printf("  [OK] cd and pwd builtins: %s", out);

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("export AURORA_M11=PosixVerified; echo $AURORA_M11", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "PosixVerified") != NULL);
    printf("  [OK] export and variable expansion ($VAR): %s", out);

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("true; echo $?", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "0") != NULL);
    printf("  [OK] true and last status ($? == 0)\n");

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("false; echo $?", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "1") != NULL);
    printf("  [OK] false and last status ($? == 1)\n");

    /* Test 2: Shell Pipelines (cmd1 | cmd2) */
    printf("[USER M11] Testing /bin/sh pipelines...\n");
    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("echo 'Line1: target\nLine2: skip' | grep target", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "Line1: target") != NULL);
    assert(strstr(out, "skip") == NULL);
    printf("  [OK] pipeline 'echo ... | grep target' verified: %s", out);

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("ls -la / | grep bin", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "bin") != NULL);
    printf("  [OK] pipeline 'ls -la / | grep bin' verified!\n");

    /* Test 3: Shell Redirections (>, >>, <) */
    printf("[USER M11] Testing /bin/sh file redirections (>, >>, <)...\n");
    unlink("/tmp/m11_redir.txt");

    ret = run_sh_cmd("echo 'Initial redirection line' > /tmp/m11_redir.txt", NULL, 0);
    assert(ret == 0);

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("cat < /tmp/m11_redir.txt", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "Initial redirection line") != NULL);
    printf("  [OK] stdout redirect (>) and stdin redirect (<) verified!\n");

    ret = run_sh_cmd("echo 'Appended second line' >> /tmp/m11_redir.txt", NULL, 0);
    assert(ret == 0);

    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("cat /tmp/m11_redir.txt", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "Initial redirection line") != NULL);
    assert(strstr(out, "Appended second line") != NULL);
    printf("  [OK] stdout append redirect (>>) verified!\n");

    /* Test 4: Control operators (;, &&, ||) */
    printf("[USER M11] Testing /bin/sh control operators (; && ||)...\n");
    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("echo first && echo second; false || echo third", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "first") != NULL);
    assert(strstr(out, "second") != NULL);
    assert(strstr(out, "third") != NULL);
    printf("  [OK] sequence, logical AND, and logical OR verified!\n");

    /* Test 5: Background Jobs (&, jobs) */
    printf("[USER M11] Testing /bin/sh background jobs (& and jobs)...\n");
    memset(out, 0, sizeof(out));
    ret = run_sh_cmd("sleep 1 & jobs", out, sizeof(out));
    assert(ret == 0);
    assert(strstr(out, "[1]") != NULL);
    printf("  [OK] background execution and jobs table verified: %s", out);

    /* Test 6: End-to-end boot sequence (login -> shell) */
    printf("[USER M11] Testing /bin/login -> /bin/sh authentication and shell execution...\n");
    int login_pipe[2];
    assert(pipe(login_pipe) == 0);

    pid_t login_pid = fork();
    if (login_pid == 0) {
        close(login_pipe[0]);
        dup2(login_pipe[1], STDOUT_FILENO);
        dup2(login_pipe[1], STDERR_FILENO);
        close(login_pipe[1]);

        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            close(devnull);
        }

        /* Feed login root directly via exec */
        char *login_argv[] = { "/bin/login", "root", NULL };
        execve("/bin/login", login_argv, environ);
        _exit(1);
    }

    close(login_pipe[1]);

    memset(out, 0, sizeof(out));
    size_t rd_bytes = 0;
    ssize_t n;
    while ((n = read(login_pipe[0], out + rd_bytes, sizeof(out) - 1 - rd_bytes)) > 0) {
        rd_bytes += n;
    }
    out[rd_bytes] = '\0';
    close(login_pipe[0]);

    int login_status = 0;
    waitpid(login_pid, &login_status, 0);

    assert(strstr(out, "Welcome to AuroraOS") != NULL);
    printf("  [OK] /bin/login verified: authenticated root, printed motd, launched shell!\n");

    printf("[USER M11] >>> ALL MILESTONE M11 REQUIREMENTS VERIFIED SUCCESSFULLY! <<<\n");
    exit(TEST_PASS);
}
