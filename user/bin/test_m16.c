/*
 * AuroraOS Milestone M16 Acceptance Test Suite
 *
 * Validates System Services, Daemons, SysV Init Scripts, Runlevels, and Remote Shell:
 *  - Subtest 1: SysV Service Scripts Interface & Contract Verification
 *  - Subtest 2: syslogd & logger Integration (/var/log/messages, /var/log/auth.log)
 *  - Subtest 3: networking Service (/etc/init.d/networking start/status)
 *  - Subtest 4: crond & crontab Task Scheduling (/var/log/cron, /run/crond.pid)
 *  - Subtest 5: sshd Remote Shell Server, Shadow Auth & PTY Bridge (TCP port 22)
 *  - Subtest 6: Runlevels & telinit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

static int run_cmd_status(const char *cmd, const char *arg1, const char *arg2)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[5];
        argv[0] = (char *)cmd;
        argv[1] = (char *)arg1;
        argv[2] = (char *)arg2;
        argv[3] = NULL;
        execve(cmd, argv, environ);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static int file_contains(const char *path, const char *needle)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    return (strstr(buf, needle) != NULL);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n=== AuroraOS Milestone M16 System Services Test Suite ===\n");

    /* Ensure standard directory structure */
    mkdir("/run", 0755);
    mkdir("/var", 0755);
    mkdir("/var/log", 0755);
    mkdir("/var/spool", 0755);
    mkdir("/var/spool/cron", 0755);
    mkdir("/var/spool/cron/crontabs", 0755);
    mkdir("/etc", 0755);
    mkdir("/etc/init.d", 0755);

    /* =========================================================================
     * Subtest 1: SysV Service Scripts Interface & Contract Verification
     * ========================================================================= */
    printf("[TEST M16] Subtest 1: Verifying SysV service scripts interface & return codes...\n");

    /* 1.1 Status when not running MUST return 3 per LSB / SysV specification */
    unlink("/run/syslogd.pid");
    int st = run_cmd_status("/bin/sh", "/etc/init.d/syslogd", "status");
    if (st != 3) {
        fprintf(stderr, "FAILED: /etc/init.d/syslogd status returned %d (expected 3 when stopped)\n", st);
        return 1;
    }
    printf("  [OK] /etc/init.d/syslogd status returns 3 when stopped\n");

    /* 1.2 Start syslogd */
    st = run_cmd_status("/bin/sh", "/etc/init.d/syslogd", "start");
    if (st != 0) {
        fprintf(stderr, "FAILED: /etc/init.d/syslogd start returned %d\n", st);
        return 1;
    }
    sleep(1);

    /* Verify PID file exists and contains valid PID */
    int pid_fd = open("/run/syslogd.pid", O_RDONLY);
    if (pid_fd < 0) {
        fprintf(stderr, "FAILED: /run/syslogd.pid was not created by syslogd\n");
        return 1;
    }
    char pid_buf[32];
    ssize_t prd = read(pid_fd, pid_buf, sizeof(pid_buf) - 1);
    close(pid_fd);
    if (prd <= 0) {
        fprintf(stderr, "FAILED: /run/syslogd.pid is empty\n");
        return 1;
    }
    pid_buf[prd] = '\0';
    pid_t syslog_pid = atoi(pid_buf);
    if (syslog_pid <= 1) {
        fprintf(stderr, "FAILED: Invalid PID %d in /run/syslogd.pid\n", syslog_pid);
        return 1;
    }
    printf("  [OK] syslogd running with PID %d, PID file verified\n", syslog_pid);

    /* 1.3 Status when running MUST return 0 */
    st = run_cmd_status("/bin/sh", "/etc/init.d/syslogd", "status");
    if (st != 0) {
        fprintf(stderr, "FAILED: /etc/init.d/syslogd status returned %d (expected 0 when running)\n", st);
        return 1;
    }
    printf("  [OK] /etc/init.d/syslogd status returns 0 when running\n");

    /* 1.4 Restart syslogd */
    st = run_cmd_status("/bin/sh", "/etc/init.d/syslogd", "restart");
    if (st != 0) {
        fprintf(stderr, "FAILED: /etc/init.d/syslogd restart returned %d\n", st);
        return 1;
    }
    sleep(1);
    printf("  [OK] /etc/init.d/syslogd restart succeeded\n");

    /* =========================================================================
     * Subtest 2: syslogd & logger Integration
     * ========================================================================= */
    printf("[TEST M16] Subtest 2: Verifying syslogd & logger message routing...\n");

    /* Run with explicit message */
    pid_t lpid = fork();
    if (lpid == 0) {
        char *largv[] = { "/bin/logger", "-p", "auth.notice", "-t", "m16_auth", "User authentication event test", NULL };
        execve("/bin/logger", largv, environ);
        _exit(1);
    }
    waitpid(lpid, NULL, 0);

    /* Send daemon message */
    lpid = fork();
    if (lpid == 0) {
        char *largv[] = { "/bin/logger", "-p", "daemon.info", "-t", "m16_daemon", "Daemon heartbeat check ok", NULL };
        execve("/bin/logger", largv, environ);
        _exit(1);
    }
    waitpid(lpid, NULL, 0);

    sleep(1);

    if (!file_contains("/var/log/auth.log", "m16_auth") && !file_contains("/var/log/auth.log", "authentication event")) {
        fprintf(stderr, "FAILED: /var/log/auth.log does not contain auth logger message\n");
        return 1;
    }
    printf("  [OK] /var/log/auth.log captured auth logger event\n");

    if (!file_contains("/var/log/messages", "m16_daemon") && !file_contains("/var/log/messages", "heartbeat")) {
        fprintf(stderr, "FAILED: /var/log/messages does not contain daemon logger message\n");
        return 1;
    }
    printf("  [OK] /var/log/messages captured daemon logger event\n");

    /* =========================================================================
     * Subtest 3: networking Service
     * ========================================================================= */
    printf("[TEST M16] Subtest 3: Verifying networking service (/etc/init.d/networking)...\n");

    st = run_cmd_status("/bin/sh", "/etc/init.d/networking", "start");
    if (st != 0) {
        fprintf(stderr, "FAILED: /etc/init.d/networking start returned %d\n", st);
        return 1;
    }

    st = run_cmd_status("/bin/sh", "/etc/init.d/networking", "status");
    if (st != 0) {
        fprintf(stderr, "FAILED: /etc/init.d/networking status returned %d\n", st);
        return 1;
    }
    printf("  [OK] networking service started and active\n");

    /* =========================================================================
     * Subtest 4: crond & crontab Task Scheduling
     * ========================================================================= */
    printf("[TEST M16] Subtest 4: Verifying crond & crontab task scheduling...\n");

    /* 4.1 Install crontab for root */
    unlink("/tmp/m16_cron_test_done");
    int cr_fd = open("/tmp/test_crontab", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cr_fd >= 0) {
        const char *cr_entry = "* * * * * /bin/touch /tmp/m16_cron_test_done\n";
        write(cr_fd, cr_entry, strlen(cr_entry));
        close(cr_fd);
    }

    st = run_cmd_status("/bin/crontab", "/tmp/test_crontab", NULL);
    if (st != 0) {
        fprintf(stderr, "FAILED: crontab installation failed\n");
        return 1;
    }

    /* 4.2 Start crond in fast test mode (-t) */
    pid_t cpid = fork();
    if (cpid == 0) {
        char *cargv[] = { "/sbin/crond", "-t", NULL };
        execve("/sbin/crond", cargv, environ);
        _exit(1);
    }
    sleep(2);

    /* Verify PID file exists */
    if (access("/run/crond.pid", F_OK) != 0) {
        fprintf(stderr, "FAILED: /run/crond.pid not found\n");
        return 1;
    }
    printf("  [OK] crond running with PID file /run/crond.pid\n");

    /* Verify task execution */
    if (access("/tmp/m16_cron_test_done", F_OK) != 0) {
        fprintf(stderr, "FAILED: crond did not execute scheduled touch command\n");
        return 1;
    }
    printf("  [OK] Scheduled cron job successfully triggered and executed!\n");

    /* Verify cron log */
    if (!file_contains("/var/log/cron", "m16_cron_test_done") && !file_contains("/var/log/cron", "CMD")) {
        fprintf(stderr, "FAILED: /var/log/cron does not contain execution record\n");
        return 1;
    }
    printf("  [OK] /var/log/cron verified with job execution record\n");

    /* Stop crond */
    run_cmd_status("/bin/sh", "/etc/init.d/crond", "stop");

    /* =========================================================================
     * Subtest 5: sshd Remote Shell Server, Shadow Auth & PTY Bridge
     * ========================================================================= */
    printf("[TEST M16] Subtest 5: Verifying sshd TCP port 22, shadow auth & PTY allocation...\n");

    /* 5.1 Start sshd */
    st = run_cmd_status("/bin/sh", "/etc/init.d/sshd", "start");
    if (st != 0) {
        fprintf(stderr, "FAILED: /etc/init.d/sshd start returned %d\n", st);
        return 1;
    }
    sleep(1);

    if (access("/run/sshd.pid", F_OK) != 0) {
        fprintf(stderr, "FAILED: /run/sshd.pid was not created\n");
        return 1;
    }
    printf("  [OK] sshd daemon started on port 22, PID file created\n");

    /* 5.2 Connect via TCP to port 22 */
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("FAILED: socket creation for ssh client");
        return 1;
    }

    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(22);
    srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(client_sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("FAILED: connect to sshd port 22");
        close(client_sock);
        return 1;
    }
    printf("  [OK] Connected to sshd TCP port 22\n");

    /* Read SSH identification banner */
    char banner_buf[256];
    ssize_t brd = read(client_sock, banner_buf, sizeof(banner_buf) - 1);
    if (brd <= 0 || strstr(banner_buf, "SSH-2.0") == NULL) {
        fprintf(stderr, "FAILED: Invalid or missing SSH banner from server\n");
        close(client_sock);
        return 1;
    }
    banner_buf[brd] = '\0';
    printf("  [OK] Received SSH banner: %s\n", banner_buf);

    /* Read login prompt (if not already read in banner_buf) and send username */
    if (strstr(banner_buf, "login:") == NULL) {
        char prompt_buf[128];
        read(client_sock, prompt_buf, sizeof(prompt_buf));
    }
    write(client_sock, "root\n", 5);

    /* Read password prompt and send password */
    char prompt_buf[128];
    prd = read(client_sock, prompt_buf, sizeof(prompt_buf) - 1);
    if (prd > 0) prompt_buf[prd] = '\0';
    write(client_sock, "root\n", 5);

    sleep(1);

    /* Verify auth log has Accepted password and Allocated PTY */
    if (!file_contains("/var/log/auth.log", "Accepted password for root")) {
        fprintf(stderr, "FAILED: /var/log/auth.log does not confirm password authentication\n");
        close(client_sock);
        return 1;
    }
    printf("  [OK] Shadow authentication verified in /var/log/auth.log\n");

    if (!file_contains("/var/log/auth.log", "Allocated PTY")) {
        fprintf(stderr, "FAILED: /var/log/auth.log does not confirm PTY allocation\n");
        close(client_sock);
        return 1;
    }
    printf("  [OK] PTY pseudo-terminal allocation verified\n");

    /* Send remote command to execute across PTY bridge */
    unlink("/tmp/m16_ssh_remote_ok");
    const char *remote_cmd = "echo REMOTE_SHELL_ACTIVE > /tmp/m16_ssh_remote_ok\n";
    write(client_sock, remote_cmd, strlen(remote_cmd));
    sleep(1);

    if (access("/tmp/m16_ssh_remote_ok", F_OK) != 0) {
        fprintf(stderr, "FAILED: Remote command sent over SSH PTY bridge did not execute\n");
        close(client_sock);
        return 1;
    }
    printf("  [OK] Command executed over remote shell and PTY bridge verified\n");

    /* Send Ctrl-C (0x03) to test terminal signal handling */
    write(client_sock, "\003", 1);
    sleep(1);

    /* Send exit command and close socket */
    write(client_sock, "exit\n", 5);
    close(client_sock);

    /* Stop sshd */
    run_cmd_status("/bin/sh", "/etc/init.d/sshd", "stop");

    /* =========================================================================
     * Subtest 6: Runlevels & telinit
     * ========================================================================= */
    printf("[TEST M16] Subtest 6: Verifying telinit & runlevel control...\n");

    st = run_cmd_status("/sbin/telinit", "1", NULL);
    if (st != 0) {
        fprintf(stderr, "FAILED: telinit 1 returned %d\n", st);
        return 1;
    }
    printf("  [OK] telinit 1 requested single-user maintenance mode\n");

    st = run_cmd_status("/sbin/telinit", "3", NULL);
    if (st != 0) {
        fprintf(stderr, "FAILED: telinit 3 returned %d\n", st);
        return 1;
    }
    printf("  [OK] telinit 3 restored multi-user mode\n");

    printf("\n[TEST M16] PASSED: All M16 system services and runlevel tests passed!\n");
    return 0;
}
