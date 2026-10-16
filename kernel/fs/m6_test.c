#include <aurora/fs/m6_test.h>
#include <aurora/fs/vfs.h>
#include <aurora/syscall.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern char user_vfs_blob_start[];
extern char user_vfs_blob_end[];
extern void enter_usermode(uint64_t rip, uint64_t rsp);

#define VFS_USER_CODE_VIRT  0x0000000000400000ULL
#define VFS_USER_STACK_VIRT 0x00007FFFF0000000ULL

static void user_vfs_trampoline(void)
{
    enter_usermode(VFS_USER_CODE_VIRT, VFS_USER_STACK_VIRT + PAGE_SIZE - 32);
}

bool test_vfs_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M6 verification test (VFS: inode, dentry, file, open, read, write, lseek, dup2, stat, mkdir, unlink)...\n");

    /* 1. Test File Creation & Write */
    int fd = sys_open("/test_file.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        printk("<0>[TEST M6] FAILED: sys_open returned %d!\n", fd);
        return false;
    }

    const char *payload = "AuroraOS VFS Subsystem Test: Hello World from RamFS!\n";
    size_t payload_len = strlen(payload);

    int64_t written = sys_write(fd, payload, payload_len);
    if (written != (int64_t)payload_len) {
        printk("<0>[TEST M6] FAILED: sys_write wrote %ld bytes (expected %lu)!\n",
               (long)written, (unsigned long)payload_len);
        sys_close(fd);
        return false;
    }

    /* 2. Test Lseek & Read */
    int64_t seek_res = sys_lseek(fd, 0, SEEK_SET);
    if (seek_res != 0) {
        printk("<0>[TEST M6] FAILED: sys_lseek returned %ld (expected 0)!\n", (long)seek_res);
        sys_close(fd);
        return false;
    }

    char read_buf[128];
    memset(read_buf, 0, sizeof(read_buf));
    int64_t read_bytes = sys_read(fd, read_buf, sizeof(read_buf) - 1);
    if (read_bytes != (int64_t)payload_len || strcmp(read_buf, payload) != 0) {
        printk("<0>[TEST M6] FAILED: sys_read returned '%s' (%ld bytes)!\n",
               read_buf, (long)read_bytes);
        sys_close(fd);
        return false;
    }
    printk("<6>[TEST M6] OK: open, write, lseek, read verified!\n");

    /* 3. Test fstat & stat */
    struct stat st;
    if (sys_fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != (off_t)payload_len) {
        printk("<0>[TEST M6] FAILED: sys_fstat returned unexpected metadata (size %lu, mode %o)!\n",
               (unsigned long)st.st_size, (unsigned int)st.st_mode);
        sys_close(fd);
        return false;
    }

    if (sys_stat("/test_file.txt", &st) != 0 || st.st_size != (off_t)payload_len) {
        printk("<0>[TEST M6] FAILED: sys_stat returned unexpected metadata!\n");
        sys_close(fd);
        return false;
    }
    printk("<6>[TEST M6] OK: stat and fstat metadata verified!\n");

    /* 4. Test dup & dup2 */
    int dup_fd = sys_dup(fd);
    if (dup_fd < 0) {
        printk("<0>[TEST M6] FAILED: sys_dup returned %d!\n", dup_fd);
        sys_close(fd);
        return false;
    }

    int target_fd = 20;
    int dup2_res = sys_dup2(dup_fd, target_fd);
    if (dup2_res != target_fd) {
        printk("<0>[TEST M6] FAILED: sys_dup2 returned %d (expected %d)!\n", dup2_res, target_fd);
        sys_close(fd);
        sys_close(dup_fd);
        return false;
    }

    /* Seek via dup_fd to offset 10 and read via target_fd */
    sys_lseek(dup_fd, 10, SEEK_SET);
    char sub_buf[16];
    memset(sub_buf, 0, sizeof(sub_buf));
    int64_t sub_read = sys_read(target_fd, sub_buf, 5);
    if (sub_read != 5 || memcmp(sub_buf, payload + 10, 5) != 0) {
        printk("<0>[TEST M6] FAILED: Shared file position across dup/dup2 fds failed!\n");
        sys_close(fd);
        sys_close(dup_fd);
        sys_close(target_fd);
        return false;
    }

    sys_close(fd);
    sys_close(dup_fd);
    sys_close(target_fd);
    printk("<6>[TEST M6] OK: dup and dup2 fd sharing verified!\n");

    /* 5. Test mkdir and nested directory access */
    int mk_res = sys_mkdir("/mydir", 0755);
    if (mk_res != 0) {
        printk("<0>[TEST M6] FAILED: sys_mkdir returned %d!\n", mk_res);
        return false;
    }

    int nested_fd = sys_open("/mydir/nested.dat", O_CREAT | O_RDWR, 0644);
    if (nested_fd < 0) {
        printk("<0>[TEST M6] FAILED: Could not create nested file: %d!\n", nested_fd);
        return false;
    }

    sys_write(nested_fd, "NESTED_DATA", 11);
    sys_close(nested_fd);

    int re_fd = sys_open("/mydir/nested.dat", O_RDONLY, 0);
    char n_buf[32];
    memset(n_buf, 0, sizeof(n_buf));
    int64_t n_read = sys_read(re_fd, n_buf, sizeof(n_buf) - 1);
    sys_close(re_fd);

    if (n_read != 11 || strcmp(n_buf, "NESTED_DATA") != 0) {
        printk("<0>[TEST M6] FAILED: Nested file content mismatch: '%s'!\n", n_buf);
        return false;
    }
    printk("<6>[TEST M6] OK: mkdir and nested file access verified!\n");

    /* 6. Test unlink */
    int un_res = sys_unlink("/test_file.txt");
    if (un_res != 0) {
        printk("<0>[TEST M6] FAILED: sys_unlink returned %d!\n", un_res);
        return false;
    }

    int fail_fd = sys_open("/test_file.txt", O_RDONLY, 0);
    if (fail_fd >= 0) {
        printk("<0>[TEST M6] FAILED: Unlinked file was still accessible!\n");
        sys_close(fail_fd);
        return false;
    }
    printk("<6>[TEST M6] OK: unlink deleted file successfully!\n");

    /* 7. Test Ring 3 User Mode VFS syscall execution */
    printk("<6>[TEST M6] Testing Ring 3 User Mode VFS syscall execution...\n");
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        return false;
    }
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    uintptr_t code_phys = pmm_alloc_page();
    uintptr_t stack_phys = pmm_alloc_page();
    vmm_map_page(user_pml4, VFS_USER_CODE_VIRT, code_phys, PTE_USER | PTE_WRITABLE);
    vmm_map_page(user_pml4, VFS_USER_STACK_VIRT, stack_phys, PTE_USER | PTE_WRITABLE);

    size_t code_len = user_vfs_blob_end - user_vfs_blob_start;
    memcpy(vmm_phys_to_virt(code_phys), user_vfs_blob_start, code_len);
    memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);

    task_t *utask = task_create("vfs_user", user_vfs_trampoline);
    if (!utask) {
        return false;
    }

    utask->pml4 = user_pml4;
    utask->cr3 = pml4_phys;
    utask->brk_start = 0x600000ULL;
    utask->brk_current = utask->brk_start;
    sched_add_task(utask);

    while (utask->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (utask->exit_code != 88) {
        printk("<0>[TEST M6] FAILED: User VFS process exited with error code %d (expected 88)!\n",
               utask->exit_code);
        sched_remove_task(utask);
        vmm_destroy_user_space(utask->pml4);
        task_destroy(utask);
        return false;
    }

    sched_remove_task(utask);
    vmm_destroy_user_space(utask->pml4);
    task_destroy(utask);
    printk("<6>[TEST M6] OK: Ring 3 User Mode process verified VFS open, write, lseek, read, close!\n");

    /* 8. Milestone M6 Accepted */
    printk("<6>[  0.000000] Milestone M6 status: [OK] VFS: inode/dentry/file, namei, open/read/write/lseek/dup2 verified.\n");
    return true;
}
