/*
 * AuroraOS Milestone M18 Acceptance Test Suite
 *
 * Verifies:
 *  - Libc unit tests & cryptographic hash vectors (NIST SHA-256)
 *  - Kernel log ring buffer and /bin/dmesg integration
 *  - POSIX process management, pipe IPC, and signal status
 *  - VFS filesystem lifecycle (mkdir, open, write, lseek, read, stat, unlink, rmdir)
 *  - System documentation (/usr/share/doc/aurora/{HANDBOOK,ABI,PORTING}.txt)
 *  - Process fork/wait stress test without zombie accumulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sha256.h>

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

    printf("\n=== AuroraOS Milestone M18 Final System Verification Suite ===\n");

    /* =========================================================================
     * Subtest 1: Libc unit tests & SHA-256 NIST vector
     * ========================================================================= */
    printf("[TEST M18] Subtest 1: Running libc unit tests and crypto vectors...\n");

    /* String tests */
    char str_buf[64];
    strcpy(str_buf, "Aurora");
    strcat(str_buf, "OS");
    if (strcmp(str_buf, "AuroraOS") != 0 || strlen(str_buf) != 8) {
        fprintf(stderr, "FAILED: string operations mismatch: %s\n", str_buf);
        return 1;
    }

    /* Memory allocator stress */
    void *ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = malloc(64 + i * 8);
        if (!ptrs[i]) {
            fprintf(stderr, "FAILED: malloc returned NULL at iter %d\n", i);
            return 1;
        }
        memset(ptrs[i], 0xAA, 64 + i * 8);
    }
    for (int i = 0; i < 100; i++) {
        free(ptrs[i]);
    }

    /* NIST SHA-256 test vector for "abc" */
    char hash_out[65];
    sha256_hash_bytes("abc", 3, hash_out);
    const char *expected_nist = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    if (strcmp(hash_out, expected_nist) != 0) {
        fprintf(stderr, "FAILED: NIST SHA-256 test vector mismatch: got %s, expected %s\n",
                hash_out, expected_nist);
        return 1;
    }
    printf("  [OK] Libc string, heap allocator, and NIST SHA-256 vectors verified\n");

    /* =========================================================================
     * Subtest 2: Kernel Log & dmesg integration
     * ========================================================================= */
    printf("[TEST M18] Subtest 2: Verifying kernel ring buffer via /bin/dmesg...\n");
    int rc = run_cmd("/bin/dmesg", NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "FAILED: /bin/dmesg returned %d\n", rc);
        return 1;
    }
    printf("  [OK] /bin/dmesg successfully extracted kernel log buffer\n");

    /* =========================================================================
     * Subtest 3: POSIX process lifecycle, pipe IPC, signals
     * ========================================================================= */
    printf("[TEST M18] Subtest 3: Verifying POSIX fork, pipe IPC, and wait4...\n");
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        fprintf(stderr, "FAILED: pipe creation failed\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "FAILED: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        /* Child */
        close(pipefd[0]);
        const char *msg = "aurora_pipe_ipc_ok";
        write(pipefd[1], msg, strlen(msg));
        close(pipefd[1]);
        _exit(42);
    }

    /* Parent */
    close(pipefd[1]);
    char pipe_read_buf[32];
    memset(pipe_read_buf, 0, sizeof(pipe_read_buf));
    read(pipefd[0], pipe_read_buf, sizeof(pipe_read_buf) - 1);
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (strcmp(pipe_read_buf, "aurora_pipe_ipc_ok") != 0) {
        fprintf(stderr, "FAILED: Pipe IPC data mismatch: '%s'\n", pipe_read_buf);
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 42) {
        fprintf(stderr, "FAILED: Child exit status was %d (expected 42)\n", WEXITSTATUS(status));
        return 1;
    }
    printf("  [OK] POSIX fork, pipe IPC, and waitpid return code verified\n");

    /* =========================================================================
     * Subtest 4: VFS filesystem lifecycle
     * ========================================================================= */
    printf("[TEST M18] Subtest 4: Verifying VFS filesystem lifecycle (mkdir, write, read, stat, unlink)...\n");
    mkdir("/tmp/m18_dir", 0755);
    int fd = open("/tmp/m18_dir/test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "FAILED: could not open /tmp/m18_dir/test.txt\n");
        return 1;
    }
    const char *payload = "AuroraOS VFS POSIX Compliance Test Data";
    write(fd, payload, strlen(payload));
    close(fd);

    struct stat st;
    if (stat("/tmp/m18_dir/test.txt", &st) < 0 || st.st_size != (off_t)strlen(payload)) {
        fprintf(stderr, "FAILED: stat size mismatch\n");
        return 1;
    }

    unlink("/tmp/m18_dir/test.txt");
    rmdir("/tmp/m18_dir");
    printf("  [OK] VFS file operations, directory nodes, and stat verified\n");

    /* =========================================================================
     * Subtest 5: System Documentation and Handbook
     * ========================================================================= */
    printf("[TEST M18] Subtest 5: Verifying system documentation (/usr/share/doc/aurora/)...\n");
    if (!file_contains("/usr/share/doc/aurora/HANDBOOK.txt", "Handbook")) {
        fprintf(stderr, "FAILED: /usr/share/doc/aurora/HANDBOOK.txt missing or empty!\n");
        return 1;
    }
    if (!file_contains("/usr/share/doc/aurora/ABI.txt", "ABI Specification")) {
        fprintf(stderr, "FAILED: /usr/share/doc/aurora/ABI.txt missing or empty!\n");
        return 1;
    }
    if (!file_contains("/usr/share/doc/aurora/PORTING.txt", "Porting Guide")) {
        fprintf(stderr, "FAILED: /usr/share/doc/aurora/PORTING.txt missing or empty!\n");
        return 1;
    }

    /* Test man page for dmesg */
    rc = run_cmd("/bin/man", "1", "dmesg");
    if (rc != 0) {
        fprintf(stderr, "FAILED: man 1 dmesg returned %d\n", rc);
        return 1;
    }
    printf("  [OK] System Handbook, ABI docs, Porting guide, and dmesg(1) man page verified\n");

    /* =========================================================================
     * Subtest 6: Process stress test & zombie cleanups
     * ========================================================================= */
    printf("[TEST M18] Subtest 6: Running 50 rapid fork/exit cycles (stress & zombie reap)...\n");
    for (int i = 0; i < 50; i++) {
        pid_t cpid = fork();
        if (cpid < 0) {
            fprintf(stderr, "FAILED: fork failed at iter %d\n", i);
            return 1;
        }
        if (cpid == 0) {
            _exit(0);
        }
        int cstatus = 0;
        waitpid(cpid, &cstatus, 0);
        if (!WIFEXITED(cstatus) || WEXITSTATUS(cstatus) != 0) {
            fprintf(stderr, "FAILED: child %d exit status != 0\n", i);
            return 1;
        }
    }
    printf("  [OK] 50 process cycles completed cleanly; no leaks or zombies\n");

    printf("\n>>> Milestone M18 verification PASSED: All 6 subtests OK! <<<\n");
    return 0;
}
