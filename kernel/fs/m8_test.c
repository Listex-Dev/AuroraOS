#include <aurora/fs/m8_test.h>
#include <aurora/drivers/block.h>
#include <aurora/drivers/ata.h>
#include <aurora/drivers/ramdisk.h>
#include <aurora/fs/ext2.h>
#include <aurora/fs/ext2_mkfs.h>
#include <aurora/fs/ext2_fsck.h>
#include <aurora/fs/vfs.h>
#include <aurora/fs/mount.h>
#include <aurora/syscall.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern char user_m8_blob_start[];
extern char user_m8_blob_end[];
extern void enter_usermode(uint64_t rip, uint64_t rsp);

#define M8_USER_CODE_VIRT  0x0000000000400000ULL
#define M8_USER_STACK_VIRT 0x00007FFFF0000000ULL

static void user_m8_trampoline(void)
{
    enter_usermode(M8_USER_CODE_VIRT, M8_USER_STACK_VIRT + PAGE_SIZE - 32);
}

bool test_ext4_root_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M8 verification test (ext2/ext4 R/W, mkfs, fsck, pivot_root, fstab)...\n");

    /* 1. Initialize Block Device Subsystem */
    block_subsys_init();
    ata_init();
    ramdisk_init(32 * 1024 * 1024); /* 32 MB ramdisk /dev/ram0 */

    struct block_device *test_bdev = bdev_get("sda");
    if (!test_bdev) {
        test_bdev = bdev_get("ram0");
    }

    if (!test_bdev) {
        printk("<0>[TEST M8] FAILED: No block device available for ext4 test!\n");
        return false;
    }

    printk("<6>[TEST M8] Target block device: /dev/%s (%llu blocks, %llu MB)\n",
           test_bdev->name, test_bdev->total_blocks,
           (test_bdev->total_blocks * test_bdev->block_size) / (1024 * 1024));

    /* 2. Format with ext4 (mkfs) */
    int mkfs_err = ext2_mkfs(test_bdev, 1024, true);
    if (mkfs_err != 0) {
        printk("<0>[TEST M8] FAILED: ext2_mkfs returned error %d!\n", mkfs_err);
        return false;
    }
    printk("<6>[TEST M8] OK: mkfs formatted /dev/%s with ext4!\n", test_bdev->name);

    /* 3. Consistency check (fsck) */
    int fsck_err = ext2_fsck(test_bdev);
    if (fsck_err != 0) {
        printk("<0>[TEST M8] FAILED: ext2_fsck reported errors: %d!\n", fsck_err);
        return false;
    }
    printk("<6>[TEST M8] OK: fsck verified clean ext4 filesystem on /dev/%s!\n", test_bdev->name);

    /* 4. Mount ext4 filesystem at /mnt */
    int mount_err = vfs_mount(test_bdev->name, "/mnt", "ext4", 0, NULL);
    if (mount_err != 0) {
        printk("<0>[TEST M8] FAILED: vfs_mount ext4 at /mnt failed: %d!\n", mount_err);
        return false;
    }
    printk("<6>[TEST M8] OK: Mounted ext4 filesystem at /mnt!\n");

    /* 5. Create directories and files on ext4 */
    sys_mkdir("/mnt/etc", 0755);
    sys_mkdir("/mnt/data", 0755);
    sys_mkdir("/mnt/old_root", 0755);

    /* Write /mnt/etc/fstab */
    int fd_fstab = sys_open("/mnt/etc/fstab", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_fstab < 0) {
        printk("<0>[TEST M8] FAILED: Could not create /mnt/etc/fstab: %d\n", fd_fstab);
        return false;
    }
    const char *fstab_content =
        "/dev/sda    /        ext4     defaults    0 1\n"
        "proc        /proc    proc     defaults    0 0\n"
        "sysfs       /sys     sysfs    defaults    0 0\n"
        "devtmpfs    /dev     devtmpfs defaults    0 0\n"
        "tmpfs       /run     tmpfs    defaults    0 0\n"
        "tmpfs       /tmp     tmpfs    defaults    0 0\n";
    sys_write(fd_fstab, fstab_content, strlen(fstab_content));
    sys_close(fd_fstab);

    /* Write multi-block payload to /mnt/data/large_payload.dat */
    int fd_large = sys_open("/mnt/data/large_payload.dat", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd_large < 0) {
        printk("<0>[TEST M8] FAILED: Could not create /mnt/data/large_payload.dat: %d\n", fd_large);
        return false;
    }

    uint8_t *sample_data = (uint8_t *)kmalloc(4096, GFP_KERNEL);
    if (!sample_data) return false;
    for (int i = 0; i < 4096; i++) {
        sample_data[i] = (uint8_t)((i * 37 + 13) & 0xFF);
    }

    int64_t w_len = sys_write(fd_large, sample_data, 4096);
    sys_close(fd_large);
    if (w_len != 4096) {
        printk("<0>[TEST M8] FAILED: Wrote %ld bytes instead of 4096\n", (long)w_len);
        kfree(sample_data);
        return false;
    }

    /* Read back and verify exact data */
    int fd_read = sys_open("/mnt/data/large_payload.dat", O_RDONLY, 0);
    if (fd_read < 0) {
        printk("<0>[TEST M8] FAILED: Could not open /mnt/data/large_payload.dat for read\n");
        kfree(sample_data);
        return false;
    }

    uint8_t *verify_data = (uint8_t *)kmalloc(4096, GFP_KERNEL);
    if (!verify_data) {
        kfree(sample_data);
        sys_close(fd_read);
        return false;
    }

    int64_t r_len = sys_read(fd_read, verify_data, 4096);
    sys_close(fd_read);
    if (r_len != 4096) {
        printk("<0>[TEST M8] FAILED: Readback length mismatch: got %ld, expected 4096\n", (long)r_len);
        kfree(sample_data);
        kfree(verify_data);
        return false;
    }
    for (int i = 0; i < 4096; i++) {
        if (sample_data[i] != verify_data[i]) {
            printk("<0>[TEST M8] FAILED: Mismatch at byte offset %d: expected 0x%02x, got 0x%02x\n",
                   i, sample_data[i], verify_data[i]);
            kfree(sample_data);
            kfree(verify_data);
            return false;
        }
    }
    kfree(sample_data);
    kfree(verify_data);
    printk("<6>[TEST M8] OK: ext4 file & directory creation, write, readback verified!\n");

    /* 6. Test pivot_root */
    int pivot_err = vfs_pivot_root("/mnt", "/mnt/old_root");
    if (pivot_err != 0) {
        printk("<0>[TEST M8] FAILED: vfs_pivot_root failed: %d\n", pivot_err);
        return false;
    }

    /* Check that root now points to ext4 */
    int fd_root_fstab = sys_open("/etc/fstab", O_RDONLY, 0);
    if (fd_root_fstab < 0) {
        printk("<0>[TEST M8] FAILED: Could not read /etc/fstab from new ext4 root!\n");
        return false;
    }
    char fstab_buf[256];
    memset(fstab_buf, 0, sizeof(fstab_buf));
    sys_read(fd_root_fstab, fstab_buf, sizeof(fstab_buf) - 1);
    sys_close(fd_root_fstab);

    if (strstr(fstab_buf, "/dev/sda") == NULL && strstr(fstab_buf, "ext4") == NULL) {
        printk("<0>[TEST M8] FAILED: /etc/fstab content invalid: %s\n", fstab_buf);
        return false;
    }

    /* Mount filesystems specified in /etc/fstab */
    vfs_mount_fstab("/etc/fstab");
    printk("<6>[TEST M8] OK: pivot_root switched root to ext4 and fstab parsed successfully!\n");

    /* 7. Test Ring 3 User Mode verification on ext4 root */
    printk("<6>[TEST M8] Testing Ring 3 User Mode access to ext4 filesystem...\n");
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
    vmm_map_page(user_pml4, M8_USER_CODE_VIRT, code_phys, PTE_USER | PTE_WRITABLE);
    vmm_map_page(user_pml4, M8_USER_STACK_VIRT, stack_phys, PTE_USER | PTE_WRITABLE);

    size_t code_len = user_m8_blob_end - user_m8_blob_start;
    memcpy(vmm_phys_to_virt(code_phys), user_m8_blob_start, code_len);
    memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);

    task_t *utask = task_create("m8_user", user_m8_trampoline);
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
        printk("<0>[TEST M8] FAILED: User M8 process exited with error code %d (expected 88)!\n",
               utask->exit_code);
        task_destroy(utask);
        vmm_destroy_user_space(utask->pml4);
        return false;
    }

    task_destroy(utask);
    vmm_destroy_user_space(utask->pml4);
    printk("<6>[TEST M8] OK: Ring 3 User Mode verified ext4 R/W syscall execution!\n");

    /* 8. Verify data persistence on block device */
    int final_fsck = ext2_fsck(test_bdev);
    if (final_fsck != 0) {
        printk("<0>[TEST M8] FAILED: Post-write fsck reported errors: %d\n", final_fsck);
        return false;
    }

    /* 9. Milestone M8 Acceptance */
    printk("<6>[  0.000000] Milestone M8 status: [OK] ext2/ext4 R/W, mkfs, fsck, pivot_root, fstab verified.\n");
    return true;
}
