/*
 * AuroraOS Milestone M19 Acceptance Test Suite
 *
 * Tests:
 * 1. Clean kernel verification (all programs loaded from VFS via execve).
 * 2. LKM subsystem userland tools (/bin/insmod, /bin/lsmod, /bin/rmmod).
 * 3. Kernel module lifecycle (loading test_module.ko, verifying /proc/modules, unloading).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

static void check(bool condition, const char *msg)
{
    if (!condition) {
        printf("[TEST M19] FAILED: %s\n", msg);
        exit(1);
    }
    printf("[TEST M19] OK: %s\n", msg);
}

int main(void)
{
    printf("============================================================\n");
    printf("      AuroraOS Milestone M19 Acceptance Test Suite          \n");
    printf(" (Clean Kernel, Rootfs Decoupling, LKM insmod/lsmod/rmmod)  \n");
    printf("============================================================\n");

    /* Subtest 1: Verify presence of test_module.ko on filesystem */
    printf("\n--- Subtest 1: Verify /lib/modules/test_module.ko on disk ---\n");
    struct stat st;
    int sret = stat("/lib/modules/test_module.ko", &st);
    if (sret != 0) {
        /* Fallback check in /build/lib/modules or /modules */
        sret = stat("/modules/test_module.ko", &st);
    }
    check(sret == 0, "Found test_module.ko on root filesystem");
    check(st.st_size > 0, "test_module.ko has non-zero size");
    printf("  Module file size: %ld bytes\n", (long)st.st_size);

    /* Subtest 2: Check /proc/modules initial state */
    printf("\n--- Subtest 2: Check initial /proc/modules ---\n");
    int fd = open("/proc/modules", O_RDONLY);
    check(fd >= 0, "Opened /proc/modules");
    char pbuf[512];
    ssize_t pn = read(fd, pbuf, sizeof(pbuf) - 1);
    close(fd);
    check(pn >= 0, "Read /proc/modules");
    pbuf[pn >= 0 ? pn : 0] = '\0';
    check(strstr(pbuf, "test_module") == NULL, "test_module is not loaded initially");

    /* Subtest 3: Run /bin/insmod /lib/modules/test_module.ko */
    printf("\n--- Subtest 3: Execute /bin/insmod via execve ---\n");
    pid_t ins_pid = fork();
    if (ins_pid == 0) {
        char *ins_argv[] = { "/bin/insmod", "/lib/modules/test_module.ko", NULL };
        execv("/bin/insmod", ins_argv);
        _exit(127);
    }
    int ins_status = 0;
    waitpid(ins_pid, &ins_status, 0);
    check(WIFEXITED(ins_status) && WEXITSTATUS(ins_status) == 0,
          "/bin/insmod executed with exit code 0");

    /* Subtest 4: Run /bin/lsmod and inspect /proc/modules */
    printf("\n--- Subtest 4: Execute /bin/lsmod and check /proc/modules ---\n");
    pid_t ls_pid = fork();
    if (ls_pid == 0) {
        char *ls_argv[] = { "/bin/lsmod", NULL };
        execv("/bin/lsmod", ls_argv);
        _exit(127);
    }
    int ls_status = 0;
    waitpid(ls_pid, &ls_status, 0);
    check(WIFEXITED(ls_status) && WEXITSTATUS(ls_status) == 0,
          "/bin/lsmod executed with exit code 0");

    fd = open("/proc/modules", O_RDONLY);
    check(fd >= 0, "Re-opened /proc/modules");
    pn = read(fd, pbuf, sizeof(pbuf) - 1);
    close(fd);
    pbuf[pn >= 0 ? pn : 0] = '\0';
    printf("  /proc/modules content:\n%s", pbuf);
    check(strstr(pbuf, "test_module") != NULL, "test_module is present in /proc/modules");
    check(strstr(pbuf, "Live") != NULL, "test_module is in Live state");

    /* Subtest 5: Run /bin/rmmod test_module */
    printf("\n--- Subtest 5: Execute /bin/rmmod via execve ---\n");
    pid_t rm_pid = fork();
    if (rm_pid == 0) {
        char *rm_argv[] = { "/bin/rmmod", "test_module", NULL };
        execv("/bin/rmmod", rm_argv);
        _exit(127);
    }
    int rm_status = 0;
    waitpid(rm_pid, &rm_status, 0);
    check(WIFEXITED(rm_status) && WEXITSTATUS(rm_status) == 0,
          "/bin/rmmod executed with exit code 0");

    /* Subtest 6: Verify module removal in /proc/modules */
    printf("\n--- Subtest 6: Verify removal from /proc/modules ---\n");
    fd = open("/proc/modules", O_RDONLY);
    check(fd >= 0, "Opened /proc/modules after rmmod");
    pn = read(fd, pbuf, sizeof(pbuf) - 1);
    close(fd);
    pbuf[pn >= 0 ? pn : 0] = '\0';
    check(strstr(pbuf, "test_module") == NULL, "test_module is completely removed");

    printf("\n============================================================\n");
    printf("Milestone M19 status: [OK]\n");
    printf(">>> TEST PASSED: Milestone M19 Acceptance Criteria Verified! <<<\n");
    printf("============================================================\n");

    return 0;
}
