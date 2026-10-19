/*
 * AuroraOS Milestone M17 Acceptance Test Suite
 *
 * Verifies:
 *  - pkg package manager with cryptographic signature verification (DoD 19.7)
 *  - Protection against corrupted/tampered packages (rejection before unpack)
 *  - Package installation, file tracking in /var/lib/pkg/db, package removal
 *  - man utility and system manual pages across sections 1, 5, 8 (DoD 19.5)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

static int run_cmd(const char *cmd, const char *arg1, const char *arg2)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[4];
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

static int file_exists(const char *path)
{
    return access(path, F_OK) == 0;
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

    printf("\n=== AuroraOS Milestone M17 Package Management & Documentation Test ===\n");

    /* =========================================================================
     * Subtest 1: Cryptographic signature verification on valid package
     * ========================================================================= */
    printf("[TEST M17] Subtest 1: Verifying signature on valid .aurpkg archive...\n");
    int rc = run_cmd("/bin/pkg", "verify", "/packages/sample.aurpkg");
    if (rc != 0) {
        fprintf(stderr, "FAILED: pkg verify on valid package returned %d\n", rc);
        return 1;
    }
    printf("  [OK] Valid package signature verified successfully\n");

    /* =========================================================================
     * Subtest 2: Defense against tampered / corrupted package (DoD 19.7)
     * ========================================================================= */
    printf("[TEST M17] Subtest 2: Verifying rejection of corrupted package (DoD 19.7)...\n");
    unlink("/bin/aurora-extra");
    unlink("/usr/share/man/man1/aurora-extra.1");

    rc = run_cmd("/bin/pkg", "install", "/packages/sample-corrupt.aurpkg");
    if (rc == 0) {
        fprintf(stderr, "FAILED: pkg install accepted corrupted package!\n");
        return 1;
    }
    if (file_exists("/bin/aurora-extra")) {
        fprintf(stderr, "FAILED: Corrupted package extracted files onto filesystem!\n");
        return 1;
    }
    printf("  [OK] Corrupted package rejected; zero filesystem modifications made\n");

    /* =========================================================================
     * Subtest 3: Clean installation of signed package
     * ========================================================================= */
    printf("[TEST M17] Subtest 3: Installing valid signed package sample.aurpkg...\n");
    rc = run_cmd("/bin/pkg", "install", "/packages/sample.aurpkg");
    if (rc != 0) {
        fprintf(stderr, "FAILED: pkg install returned %d\n", rc);
        return 1;
    }

    if (!file_exists("/bin/aurora-extra")) {
        fprintf(stderr, "FAILED: /bin/aurora-extra was not extracted!\n");
        return 1;
    }
    if (!file_exists("/usr/share/man/man1/aurora-extra.1")) {
        fprintf(stderr, "FAILED: /usr/share/man/man1/aurora-extra.1 was not extracted!\n");
        return 1;
    }
    if (!file_contains("/var/lib/pkg/db", "Package: sample")) {
        fprintf(stderr, "FAILED: package not registered in /var/lib/pkg/db!\n");
        return 1;
    }
    printf("  [OK] Package files extracted and registered in database\n");

    /* =========================================================================
     * Subtest 4: Execute newly installed package binary
     * ========================================================================= */
    printf("[TEST M17] Subtest 4: Executing installed utility /bin/aurora-extra...\n");
    rc = run_cmd("/bin/aurora-extra", NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "FAILED: /bin/aurora-extra returned %d\n", rc);
        return 1;
    }
    printf("  [OK] Installed binary runs successfully\n");

    /* =========================================================================
     * Subtest 5: Database tracking and package inspection
     * ========================================================================= */
    printf("[TEST M17] Subtest 5: Testing 'pkg list' and 'pkg info'...\n");
    rc = run_cmd("/bin/pkg", "list", NULL);
    if (rc != 0) {
        fprintf(stderr, "FAILED: pkg list returned %d\n", rc);
        return 1;
    }
    rc = run_cmd("/bin/pkg", "info", "sample");
    if (rc != 0) {
        fprintf(stderr, "FAILED: pkg info sample returned %d\n", rc);
        return 1;
    }
    printf("  [OK] Package database tracking and inspection verified\n");

    /* =========================================================================
     * Subtest 6: Manual page system & man utility (DoD 19.5)
     * ========================================================================= */
    printf("[TEST M17] Subtest 6: Testing man documentation system (DoD 19.5)...\n");

    /* 6.1 Section 1: Core utility */
    rc = run_cmd("/bin/man", "1", "ls");
    if (rc != 0) {
        fprintf(stderr, "FAILED: man 1 ls returned %d\n", rc);
        return 1;
    }

    /* 6.2 Section 5: Configuration file format */
    rc = run_cmd("/bin/man", "5", "passwd");
    if (rc != 0) {
        fprintf(stderr, "FAILED: man 5 passwd returned %d\n", rc);
        return 1;
    }

    /* 6.3 Section 8: System daemon */
    rc = run_cmd("/bin/man", "8", "sshd");
    if (rc != 0) {
        fprintf(stderr, "FAILED: man 8 sshd returned %d\n", rc);
        return 1;
    }

    /* 6.4 Newly installed package man-page */
    rc = run_cmd("/bin/man", "1", "aurora-extra");
    if (rc != 0) {
        fprintf(stderr, "FAILED: man 1 aurora-extra returned %d\n", rc);
        return 1;
    }

    /* 6.5 Negative test: Nonexistent man page */
    rc = run_cmd("/bin/man", "nonexistent_command", NULL);
    if (rc == 0) {
        fprintf(stderr, "FAILED: man returned 0 on nonexistent command!\n");
        return 1;
    }
    printf("  [OK] man manual page viewer and sections verified\n");

    /* =========================================================================
     * Subtest 7: Package removal
     * ========================================================================= */
    printf("[TEST M17] Subtest 7: Removing package with 'pkg remove sample'...\n");
    rc = run_cmd("/bin/pkg", "remove", "sample");
    if (rc != 0) {
        fprintf(stderr, "FAILED: pkg remove sample returned %d\n", rc);
        return 1;
    }

    if (file_exists("/bin/aurora-extra")) {
        fprintf(stderr, "FAILED: /bin/aurora-extra still exists after removal!\n");
        return 1;
    }
    if (file_exists("/usr/share/man/man1/aurora-extra.1")) {
        fprintf(stderr, "FAILED: /usr/share/man/man1/aurora-extra.1 still exists after removal!\n");
        return 1;
    }
    if (file_contains("/var/lib/pkg/db", "Package: sample")) {
        fprintf(stderr, "FAILED: package sample still in /var/lib/pkg/db after removal!\n");
        return 1;
    }
    printf("  [OK] Package removed cleanly and unregistered from database\n");

    printf("\n>>> Milestone M17 verification PASSED: All 7 subtests OK! <<<\n");
    return 0;
}
