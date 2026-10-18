#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

static void check(bool condition, const char *msg)
{
    if (!condition) {
        printf("[TEST M12] FAILED: %s\n", msg);
        exit(1);
    }
    printf("[TEST M12] OK: %s\n", msg);
}

int main(void)
{
    printf("============================================================\n");
    printf("      AuroraOS Milestone M12 Acceptance Test Suite          \n");
    printf("  (PCI Bus, Block Devices, Partitions, mknod, mount /dev/sda2) \n");
    printf("============================================================\n");

    /* Subtest 1: PCI Bus Subsystem in /sys/devices */
    printf("\n--- Subtest 1: PCI Bus Subsystem Verification ---\n");
    DIR *pci_dir = opendir("/sys/devices/pci0000:00");
    check(pci_dir != NULL, "Opened /sys/devices/pci0000:00 directory");
    int pci_dev_count = 0;
    struct dirent *ent;
    while ((ent = readdir(pci_dir)) != NULL) {
        if (ent->d_name[0] != '.') {
            pci_dev_count++;
            printf("  [PCI] Found device in sysfs: %s\n", ent->d_name);
        }
    }
    closedir(pci_dir);
    check(pci_dev_count > 0, "Discovered PCI device nodes in sysfs");

    /* Subtest 2: /proc/partitions and /proc/devices */
    printf("\n--- Subtest 2: /proc/partitions and /proc/devices ---\n");
    int pfd = open("/proc/partitions", O_RDONLY);
    check(pfd >= 0, "Opened /proc/partitions");
    char pbuf[1024];
    ssize_t pn = read(pfd, pbuf, sizeof(pbuf) - 1);
    close(pfd);
    check(pn > 0, "Read /proc/partitions content");
    pbuf[pn] = '\0';
    printf("%s\n", pbuf);
    check(strstr(pbuf, "sda") != NULL, "sda found in /proc/partitions");
    check(strstr(pbuf, "sda1") != NULL, "sda1 found in /proc/partitions");
    check(strstr(pbuf, "sda2") != NULL, "sda2 found in /proc/partitions");

    int dfd = open("/proc/devices", O_RDONLY);
    check(dfd >= 0, "Opened /proc/devices");
    char dbuf[512];
    ssize_t dn = read(dfd, dbuf, sizeof(dbuf) - 1);
    close(dfd);
    check(dn > 0, "Read /proc/devices content");
    dbuf[dn] = '\0';
    check(strstr(dbuf, "sd") != NULL, "Major 8 (sd) registered in /proc/devices");

    /* Subtest 3: fdisk -l utility */
    printf("\n--- Subtest 3: fdisk -l Partition Scanner ---\n");
    pid_t fdisk_pid = fork();
    if (fdisk_pid == 0) {
        char *fdisk_argv[] = { "/bin/fdisk", "-l", "/dev/sda", NULL };
        execv("/bin/fdisk", fdisk_argv);
        _exit(127);
    }
    int fdisk_status = 0;
    waitpid(fdisk_pid, &fdisk_status, 0);
    check(WIFEXITED(fdisk_status) && WEXITSTATUS(fdisk_status) == 0,
          "/bin/fdisk -l /dev/sda executed with exit code 0");

    /* Subtest 4: mknod character devices */
    printf("\n--- Subtest 4: mknod Character Device Nodes ---\n");
    int err = mknod("/dev/test_null", S_IFCHR | 0666, makedev(1, 3));
    check(err == 0, "mknod /dev/test_null c 1 3 returned 0");

    struct stat st_null;
    check(stat("/dev/test_null", &st_null) == 0, "stat(/dev/test_null) succeeded");
    check(S_ISCHR(st_null.st_mode), "/dev/test_null is S_IFCHR");
    check(st_null.st_rdev == makedev(1, 3), "/dev/test_null has rdev 1, 3");

    int tnull_fd = open("/dev/test_null", O_RDWR);
    check(tnull_fd >= 0, "Opened /dev/test_null");
    char dummy[16];
    ssize_t null_rd = read(tnull_fd, dummy, sizeof(dummy));
    check(null_rd == 0, "Read from /dev/test_null returned 0 (EOF)");
    ssize_t null_wr = write(tnull_fd, "test", 4);
    check(null_wr == 4, "Write to /dev/test_null returned 4");
    close(tnull_fd);

    err = mknod("/dev/test_zero", S_IFCHR | 0666, makedev(1, 5));
    check(err == 0, "mknod /dev/test_zero c 1 5 returned 0");
    int tzero_fd = open("/dev/test_zero", O_RDONLY);
    check(tzero_fd >= 0, "Opened /dev/test_zero");
    char zeros[32];
    memset(zeros, 0xFF, sizeof(zeros));
    ssize_t zero_rd = read(tzero_fd, zeros, sizeof(zeros));
    check(zero_rd == sizeof(zeros), "Read 32 bytes from /dev/test_zero");
    bool all_zero = true;
    for (size_t i = 0; i < sizeof(zeros); i++) {
        if (zeros[i] != 0) { all_zero = false; break; }
    }
    check(all_zero, "All 32 bytes from /dev/test_zero are 0x00");
    close(tzero_fd);

    /* Subtest 5: mknod block devices */
    printf("\n--- Subtest 5: mknod Block Device Node ---\n");
    err = mknod("/dev/test_sda2", S_IFBLK | 0660, makedev(8, 2));
    check(err == 0, "mknod /dev/test_sda2 b 8 2 returned 0");

    struct stat st_bdev;
    check(stat("/dev/test_sda2", &st_bdev) == 0, "stat(/dev/test_sda2) succeeded");
    check(S_ISBLK(st_bdev.st_mode), "/dev/test_sda2 is S_IFBLK");
    check(st_bdev.st_rdev == makedev(8, 2), "/dev/test_sda2 has rdev 8, 2");

    int bdev_fd = open("/dev/test_sda2", O_RDONLY);
    check(bdev_fd >= 0, "Opened /dev/test_sda2");
    /* Read sector 2 (offset 1024) to inspect ext2/ext4 superblock magic 0xEF53 */
    lseek(bdev_fd, 1024, SEEK_SET);
    uint8_t sb_buf[1024];
    ssize_t sb_read = read(bdev_fd, sb_buf, sizeof(sb_buf));
    check(sb_read == sizeof(sb_buf), "Read superblock from /dev/test_sda2");
    uint16_t magic = (uint16_t)sb_buf[56] | ((uint16_t)sb_buf[57] << 8);
    printf("  [EXT4] Superblock magic read from /dev/test_sda2: 0x%04x\n", magic);
    check(magic == 0xEF53, "Filesystem magic matches ext2/ext4 0xEF53");
    close(bdev_fd);

    /* Subtest 6: mount /dev/sda2 /mnt and Filesystem R/W */
    printf("\n--- Subtest 6: mount /dev/sda2 /mnt and Filesystem R/W ---\n");
    mkdir("/mnt", 0755);

    pid_t mount_pid = fork();
    if (mount_pid == 0) {
        char *mount_argv[] = { "/bin/mount", "/dev/sda2", "/mnt", NULL };
        execv("/bin/mount", mount_argv);
        _exit(127);
    }
    int mount_status = 0;
    waitpid(mount_pid, &mount_status, 0);
    check(WIFEXITED(mount_status) && WEXITSTATUS(mount_status) == 0,
          "/bin/mount /dev/sda2 /mnt executed with exit code 0");

    /* Verify /mnt in /proc/mounts */
    int mfd = open("/proc/mounts", O_RDONLY);
    check(mfd >= 0, "Opened /proc/mounts");
    char mbuf[1024];
    ssize_t mn = read(mfd, mbuf, sizeof(mbuf) - 1);
    close(mfd);
    check(mn > 0, "Read /proc/mounts");
    mbuf[mn] = '\0';
    printf("%s\n", mbuf);
    check(strstr(mbuf, "/mnt") != NULL, "/mnt verified in active mount table");

    /* Create test file on /mnt */
    const char *test_data = "AuroraOS Milestone M12 Block Device Subsystem PASSED!\n";
    int wfd = open("/mnt/m12_verify.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    check(wfd >= 0, "Created /mnt/m12_verify.txt");
    ssize_t wr_bytes = write(wfd, test_data, strlen(test_data));
    check(wr_bytes == (ssize_t)strlen(test_data), "Wrote verification string to /mnt/m12_verify.txt");
    close(wfd);

    /* Read back test file from /mnt */
    int rfd = open("/mnt/m12_verify.txt", O_RDONLY);
    check(rfd >= 0, "Reopened /mnt/m12_verify.txt for reading");
    char read_back[128];
    ssize_t rd_bytes = read(rfd, read_back, sizeof(read_back) - 1);
    close(rfd);
    check(rd_bytes > 0, "Read data back from /mnt/m12_verify.txt");
    read_back[rd_bytes] = '\0';
    printf("  Read content: %s", read_back);
    check(strcmp(read_back, test_data) == 0, "Data matches original verification string exactly");

    /* Subtest 7: ls -l /dev formatting */
    printf("\n--- Subtest 7: ls -l Device Node Major/Minor Verification ---\n");
    struct stat st_sda, st_sda1, st_sda2;
    check(stat("/dev/sda", &st_sda) == 0, "stat /dev/sda");
    check(major(st_sda.st_rdev) == 8 && minor(st_sda.st_rdev) == 0, "sda has major 8, minor 0");
    check(stat("/dev/sda1", &st_sda1) == 0, "stat /dev/sda1");
    check(major(st_sda1.st_rdev) == 8 && minor(st_sda1.st_rdev) == 1, "sda1 has major 8, minor 1");
    check(stat("/dev/sda2", &st_sda2) == 0, "stat /dev/sda2");
    check(major(st_sda2.st_rdev) == 8 && minor(st_sda2.st_rdev) == 2, "sda2 has major 8, minor 2");

    printf("\n============================================================\n");
    printf("Milestone M12 status: [OK]\n");
    printf(">>> TEST PASSED: Milestone M12 Acceptance Criteria Verified! <<<\n");
    printf("============================================================\n");

    return 0;
}
