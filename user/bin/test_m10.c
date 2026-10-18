#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

static int run_cmd(const char *path, char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        printf("[USER M10] fork failed for %s\n", path);
        return -1;
    }
    if (pid == 0) {
        execv(path, argv);
        printf("[USER M10] execv failed for %s\n", path);
        _exit(127);
    }
    int status = 0;
    pid_t reaped = waitpid(pid, &status, 0);
    if (reaped != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("[USER M10] command %s failed: reaped=%d, exit=%d\n",
               path, reaped, WEXITSTATUS(status));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("[USER M10] Starting Milestone M10 verification in Ring 3 user mode...\n");

    /* 1. Verify libc malloc, calloc, realloc, free */
    void *ptr = malloc(128);
    if (!ptr) {
        printf("[USER M10] FAILED: malloc returned NULL!\n");
        exit(1);
    }
    strcpy((char *)ptr, "AuroraOS libc malloc verification string");
    ptr = realloc(ptr, 512);
    if (!ptr || strcmp((char *)ptr, "AuroraOS libc malloc verification string") != 0) {
        printf("[USER M10] FAILED: realloc failed!\n");
        exit(2);
    }
    free(ptr);
    printf("[USER M10] OK: libc malloc/free/realloc verified!\n");

    /* 2. Verify libc stdio file operations */
    FILE *f = fopen("/tmp/cat_input.txt", "w");
    if (!f) {
        printf("[USER M10] FAILED: fopen for writing failed!\n");
        exit(3);
    }
    fputs("Line 1: AuroraOS cat test\nLine 2: Target keyword match\n", f);
    fclose(f);

    f = fopen("/tmp/cat_input.txt", "r");
    if (!f) {
        printf("[USER M10] FAILED: fopen for reading failed!\n");
        exit(4);
    }
    char linebuf[128];
    if (!fgets(linebuf, sizeof(linebuf), f)) {
        printf("[USER M10] FAILED: fgets failed!\n");
        exit(5);
    }
    fclose(f);
    printf("[USER M10] OK: libc stdio fopen/fputs/fgets verified!\n");

    /* 3. Verify /bin/ls */
    printf("[USER M10] Running /bin/ls -la /tmp...\n");
    char *ls_args[] = { "/bin/ls", "-la", "/tmp", NULL };
    if (run_cmd("/bin/ls", ls_args) != 0) {
        exit(10);
    }
    printf("[USER M10] OK: /bin/ls verified!\n");

    /* 4. Verify /bin/cat */
    printf("[USER M10] Running /bin/cat /tmp/cat_input.txt...\n");
    char *cat_args[] = { "/bin/cat", "/tmp/cat_input.txt", NULL };
    if (run_cmd("/bin/cat", cat_args) != 0) {
        exit(11);
    }
    printf("[USER M10] OK: /bin/cat verified!\n");

    /* 5. Verify /bin/cp */
    printf("[USER M10] Running /bin/cp /tmp/cat_input.txt /tmp/cat_copy.txt...\n");
    char *cp_args[] = { "/bin/cp", "/tmp/cat_input.txt", "/tmp/cat_copy.txt", NULL };
    if (run_cmd("/bin/cp", cp_args) != 0) {
        exit(12);
    }
    struct stat st;
    if (stat("/tmp/cat_copy.txt", &st) != 0 || st.st_size == 0) {
        printf("[USER M10] FAILED: /tmp/cat_copy.txt not created or empty!\n");
        exit(13);
    }
    printf("[USER M10] OK: /bin/cp verified!\n");

    /* 6. Verify /bin/grep */
    printf("[USER M10] Running /bin/grep keyword /tmp/cat_copy.txt...\n");
    char *grep_args[] = { "/bin/grep", "keyword", "/tmp/cat_copy.txt", NULL };
    if (run_cmd("/bin/grep", grep_args) != 0) {
        exit(14);
    }
    printf("[USER M10] OK: /bin/grep verified!\n");

    /* 7. Verify /bin/ps */
    printf("[USER M10] Running /bin/ps...\n");
    char *ps_args[] = { "/bin/ps", NULL };
    if (run_cmd("/bin/ps", ps_args) != 0) {
        exit(15);
    }
    printf("[USER M10] OK: /bin/ps verified!\n");

    /* 8. Verify /bin/vi */
    printf("[USER M10] Running /bin/vi -c wq /tmp/vi_out.txt...\n");
    char *vi_args[] = { "/bin/vi", "-c", "wq", "/tmp/vi_out.txt", NULL };
    if (run_cmd("/bin/vi", vi_args) != 0) {
        exit(16);
    }
    if (stat("/tmp/vi_out.txt", &st) != 0) {
        printf("[USER M10] FAILED: /tmp/vi_out.txt was not created by vi!\n");
        exit(17);
    }
    printf("[USER M10] OK: /bin/vi verified!\n");

    printf("[USER M10] >>> ALL 6 USERLAND UTILITIES VERIFIED SUCCESSFULLY! <<<\n");
    exit(100);
}
