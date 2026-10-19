/*
 * AuroraOS Milestone M15 Acceptance Test Suite
 * Validates Security Subsystem:
 *  - Subtest 1: /etc/shadow DAC permissions (0640, root-only access)
 *  - Subtest 2: umask syscall & file creation permissions
 *  - Subtest 3: Process credentials & SHA-512 crypt password verification
 *  - Subtest 4: Stack canary buffer overflow trap (SIGABRT)
 *  - Subtest 5: NX (No-Execute) stack protection (SIGSEGV)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <shadow.h>
#include <crypt.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

/* Helper for Subtest 4: stack smashing */
__attribute__((noinline)) static void trigger_stack_smash(void)
{
    char buf[8];
    __asm__ volatile(
        "mov %0, %%rdi\n\t"
        "mov $64, %%ecx\n\t"
        "mov $0x41, %%al\n\t"
        "rep stosb"
        :
        : "r"(buf)
        : "rdi", "rcx", "rax", "memory"
    );
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n=== AuroraOS Milestone M15 Security Test Suite ===\n");

    /* =========================================================================
     * Subtest 1: /etc/shadow DAC Permissions
     * ========================================================================= */
    printf("[TEST M15] Subtest 1: Verifying /etc/shadow DAC permissions (0640)...\n");

    /* 1.1 Check as root: must be readable */
    int root_fd = open("/etc/shadow", O_RDONLY);
    if (root_fd < 0) {
        fprintf(stderr, "FAILED: Root unable to open /etc/shadow: %s\n", strerror(errno));
        return 1;
    }
    char shadow_buf[512];
    ssize_t s_rd = read(root_fd, shadow_buf, sizeof(shadow_buf) - 1);
    close(root_fd);
    if (s_rd <= 0 || strstr(shadow_buf, "$6$") == NULL) {
        fprintf(stderr, "FAILED: /etc/shadow does not contain valid SHA-512 hashes\n");
        return 1;
    }

    struct spwd *root_sp = getspnam("root");
    if (!root_sp || !root_sp->sp_pwdp) {
        fprintf(stderr, "FAILED: getspnam('root') returned NULL for root caller\n");
        return 1;
    }

    /* 1.2 Fork child, drop privileges to UID 1000, attempt access: MUST return EACCES */
    pid_t child1 = fork();
    if (child1 < 0) {
        perror("fork child1 failed");
        return 1;
    }

    if (child1 == 0) {
        /* Child: switch to non-root user (aurora) */
        if (setregid(1000, 1000) != 0 || setreuid(1000, 1000) != 0) {
            _exit(10);
        }

        /* Try opening /etc/shadow: MUST fail with EACCES */
        int denied_fd = open("/etc/shadow", O_RDONLY);
        if (denied_fd >= 0) {
            close(denied_fd);
            _exit(11); /* Unexpectedly succeeded */
        }
        if (errno != EACCES) {
            _exit(12); /* Failed with wrong errno */
        }

        /* getspnam must also fail (return NULL) for unprivileged user */
        if (getspnam("root") != NULL) {
            _exit(13);
        }

        _exit(0);
    }

    int status1 = 0;
    wait4(child1, &status1, 0, NULL);
    if (status1 != 0) {
        fprintf(stderr, "FAILED: Subtest 1 child exited with status %d (expected 0)\n", status1);
        return 1;
    }
    printf("[TEST M15] Subtest 1: /etc/shadow DAC permissions passed [OK]\n");

    /* =========================================================================
     * Subtest 2: umask syscall & file creation modes
     * ========================================================================= */
    printf("[TEST M15] Subtest 2: Verifying umask syscall & mode enforcement...\n");

    mode_t old_mask = umask(0022);
    (void)old_mask;

    unlink("/tmp/m15_umask_test1.txt");
    int u1 = open("/tmp/m15_umask_test1.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (u1 < 0) {
        fprintf(stderr, "FAILED: open test1: %s\n", strerror(errno));
        return 1;
    }
    close(u1);

    struct stat st1;
    if (stat("/tmp/m15_umask_test1.txt", &st1) != 0) {
        fprintf(stderr, "FAILED: stat test1: %s\n", strerror(errno));
        return 1;
    }
    if ((st1.st_mode & 0777) != 0644) {
        fprintf(stderr, "FAILED: Expected mode 0644 with umask 0022, got 0%o\n", (unsigned)(st1.st_mode & 0777));
        return 1;
    }

    /* Change umask to 0077: new file should have mode 0600 */
    mode_t prev = umask(0077);
    if ((prev & 0777) != 0022) {
        fprintf(stderr, "FAILED: umask(0077) did not return old mask 0022 (got 0%o)\n", (unsigned)prev);
        return 1;
    }

    unlink("/tmp/m15_umask_test2.txt");
    int u2 = open("/tmp/m15_umask_test2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (u2 < 0) {
        fprintf(stderr, "FAILED: open test2: %s\n", strerror(errno));
        return 1;
    }
    close(u2);

    struct stat st2;
    if (stat("/tmp/m15_umask_test2.txt", &st2) != 0) {
        fprintf(stderr, "FAILED: stat test2: %s\n", strerror(errno));
        return 1;
    }
    if ((st2.st_mode & 0777) != 0600) {
        fprintf(stderr, "FAILED: Expected mode 0600 with umask 0077, got 0%o\n", (unsigned)(st2.st_mode & 0777));
        return 1;
    }

    /* Restore umask to 0022 */
    prev = umask(0022);
    if ((prev & 0777) != 0077) {
        fprintf(stderr, "FAILED: umask(0022) did not return old mask 0077 (got 0%o)\n", (unsigned)prev);
        return 1;
    }

    unlink("/tmp/m15_umask_test1.txt");
    unlink("/tmp/m15_umask_test2.txt");
    printf("[TEST M15] Subtest 2: umask and file creation permissions passed [OK]\n");

    /* =========================================================================
     * Subtest 3: Credentials & SHA-512 crypt password verification
     * ========================================================================= */
    printf("[TEST M15] Subtest 3: Verifying credentials and SHA-512 password crypt...\n");

    if (getuid() != 0 || geteuid() != 0) {
        fprintf(stderr, "FAILED: Initial process is not root (uid=%u, euid=%u)\n", getuid(), geteuid());
        return 1;
    }

    /* Test password hashing */
    const char *expected_root_hash = "$6$saltsalt$FCtXpTmG7SvkmMFnnZIotZMsxPcyMf9rYNlYFfsA2Fau3J9awLnj6tPMJW6qxQdpHAgjs0cvMWQ2Ve2HWIdX2/";
    char *hash1 = crypt("root", "$6$saltsalt$");
    if (!hash1 || strcmp(hash1, expected_root_hash) != 0) {
        fprintf(stderr, "FAILED: crypt('root') mismatch (got '%s')\n", hash1 ? hash1 : "NULL");
        return 1;
    }

    char *hash_wrong = crypt("invalidpassword", "$6$saltsalt$");
    if (!hash_wrong || strcmp(hash_wrong, expected_root_hash) == 0) {
        fprintf(stderr, "FAILED: crypt('invalidpassword') should not match root hash\n");
        return 1;
    }

    /* Test credential transition & restriction on non-root process */
    pid_t child3 = fork();
    if (child3 < 0) {
        perror("fork child3 failed");
        return 1;
    }

    if (child3 == 0) {
        /* Child switches to aurora */
        if (setreuid(1000, 1000) != 0) {
            _exit(20);
        }
        if (getuid() != 1000 || geteuid() != 1000) {
            _exit(21);
        }

        /* Non-root process attempting to setuid(0) MUST fail with EPERM */
        if (setuid(0) == 0) {
            _exit(22); /* Succeeded without permission */
        }
        if (errno != EPERM) {
            _exit(23); /* Wrong errno */
        }

        _exit(0);
    }

    int status3 = 0;
    wait4(child3, &status3, 0, NULL);
    if (status3 != 0) {
        fprintf(stderr, "FAILED: Subtest 3 child exited with status %d (expected 0)\n", status3);
        return 1;
    }
    printf("[TEST M15] Subtest 3: Credentials and password crypt verification passed [OK]\n");

    /* =========================================================================
     * Subtest 4: Stack canary buffer overflow trap
     * ========================================================================= */
    printf("[TEST M15] Subtest 4: Verifying stack canary buffer overflow protection...\n");

    pid_t child4 = fork();
    if (child4 < 0) {
        perror("fork child4 failed");
        return 1;
    }

    if (child4 == 0) {
        /* Child deliberately smashes its stack frame */
        trigger_stack_smash();
        /* If canary failed to catch it, child would reach here */
        _exit(0);
    }

    int status4 = 0;
    wait4(child4, &status4, 0, NULL);

    /* Child MUST have been terminated by SIGABRT (wait4 status & 0x7F == SIGABRT) */
    if ((status4 & 0x7F) != SIGABRT && status4 != 134) {
        fprintf(stderr, "FAILED: Child did not terminate via SIGABRT (status=%d, expected SIGABRT)\n", status4);
        return 1;
    }
    printf("[TEST M15] Subtest 4: Stack canary buffer overflow trap caught with SIGABRT [OK]\n");

    /* =========================================================================
     * Subtest 5: NX (No-Execute) stack protection
     * ========================================================================= */
    printf("[TEST M15] Subtest 5: Verifying NX (No-Execute) stack protection...\n");

    pid_t child5 = fork();
    if (child5 < 0) {
        perror("fork child5 failed");
        return 1;
    }

    if (child5 == 0) {
        /* Allocate executable code on user stack: single 'ret' opcode 0xC3 */
        volatile uint8_t code_on_stack[16];
        code_on_stack[0] = 0xC3; /* ret */

        /* Attempt to call function pointer on stack */
        void (*fn)(void) = (void (*)(void))code_on_stack;
        fn();

        /* If NX was disabled, fn() returned normally */
        _exit(42);
    }

    int status5 = 0;
    wait4(child5, &status5, 0, NULL);

    /* Child MUST have been terminated by SIGSEGV (wait4 status & 0x7F == SIGSEGV) */
    if ((status5 & 0x7F) != SIGSEGV && status5 != 139) {
        fprintf(stderr, "FAILED: NX execution on stack did not trigger SIGSEGV (status=%d, expected SIGSEGV)\n", status5);
        return 1;
    }
    printf("[TEST M15] Subtest 5: NX non-executable stack protection caught with SIGSEGV [OK]\n");

    printf("\n======================================================\n");
    printf("  All Milestone M15 Security tests PASSED!\n");
    printf("======================================================\n\n");

    return 0;
}
