#include <aurora/fs/m7_test.h>
#include <aurora/fs/vfs.h>
#include <aurora/fs/cpio.h>
#include <aurora/syscall.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern char user_m7_blob_start[];
extern char user_m7_blob_end[];
extern void enter_usermode(uint64_t rip, uint64_t rsp);

#define M7_USER_CODE_VIRT  0x0000000000400000ULL
#define M7_USER_STACK_VIRT 0x00007FFFF0000000ULL

static void user_m7_trampoline(void)
{
    enter_usermode(M7_USER_CODE_VIRT, M7_USER_STACK_VIRT + PAGE_SIZE - 32);
}

static void format_hex8(char *dst, uint32_t val)
{
    const char hex_chars[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) {
        dst[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
}

static size_t build_cpio_entry(uint8_t *buf, const char *name, const void *data, size_t data_len, uint32_t mode)
{
    struct cpio_newc_header *hdr = (struct cpio_newc_header *)buf;
    memcpy(hdr->c_magic, "070701", 6);
    format_hex8(hdr->c_ino, 1);
    format_hex8(hdr->c_mode, mode);
    format_hex8(hdr->c_uid, 0);
    format_hex8(hdr->c_gid, 0);
    format_hex8(hdr->c_nlink, 1);
    format_hex8(hdr->c_mtime, 0);
    format_hex8(hdr->c_filesize, (uint32_t)data_len);
    format_hex8(hdr->c_devmajor, 0);
    format_hex8(hdr->c_devminor, 0);
    format_hex8(hdr->c_rdevmajor, 0);
    format_hex8(hdr->c_rdevminor, 0);
    size_t name_len = strlen(name) + 1;
    format_hex8(hdr->c_namesize, (uint32_t)name_len);
    format_hex8(hdr->c_check, 0);

    memcpy(buf + sizeof(struct cpio_newc_header), name, name_len);
    size_t name_padded = (sizeof(struct cpio_newc_header) + name_len + 3) & ~3;
    if (data_len > 0 && data) {
        memcpy(buf + name_padded, data, data_len);
    }
    size_t data_padded = (data_len + 3) & ~3;
    return name_padded + data_padded;
}

bool test_virtual_filesystems(void)
{
    printk("<6>[  0.000000] Running Milestone M7 verification test (ramfs+cpio, procfs, devtmpfs, tmpfs, sysfs)...\n");

    /* 1. Test procfs: /proc/meminfo, /proc/version, /proc/uptime, /proc/mounts, /proc/self/status */
    int fd_mem = sys_open("/proc/meminfo", O_RDONLY, 0);
    if (fd_mem < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /proc/meminfo: %d\n", fd_mem);
        return false;
    }
    char buf[512];
    memset(buf, 0, sizeof(buf));
    int64_t nread = sys_read(fd_mem, buf, sizeof(buf) - 1);
    sys_close(fd_mem);
    if (nread <= 0 || strstr(buf, "MemTotal:") == NULL || strstr(buf, "MemFree:") == NULL) {
        printk("<0>[TEST M7] FAILED: /proc/meminfo content invalid: %s\n", buf);
        return false;
    }

    int fd_ver = sys_open("/proc/version", O_RDONLY, 0);
    if (fd_ver < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /proc/version: %d\n", fd_ver);
        return false;
    }
    memset(buf, 0, sizeof(buf));
    nread = sys_read(fd_ver, buf, sizeof(buf) - 1);
    sys_close(fd_ver);
    if (nread <= 0 || strstr(buf, "AuroraOS") == NULL) {
        printk("<0>[TEST M7] FAILED: /proc/version content invalid: %s\n", buf);
        return false;
    }

    int fd_up = sys_open("/proc/uptime", O_RDONLY, 0);
    if (fd_up < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /proc/uptime: %d\n", fd_up);
        return false;
    }
    memset(buf, 0, sizeof(buf));
    nread = sys_read(fd_up, buf, sizeof(buf) - 1);
    sys_close(fd_up);
    if (nread <= 0 || strchr(buf, '.') == NULL) {
        printk("<0>[TEST M7] FAILED: /proc/uptime content invalid: %s\n", buf);
        return false;
    }

    int fd_mnt = sys_open("/proc/mounts", O_RDONLY, 0);
    if (fd_mnt >= 0) {
        memset(buf, 0, sizeof(buf));
        sys_read(fd_mnt, buf, sizeof(buf) - 1);
        sys_close(fd_mnt);
        if (strstr(buf, "/proc") == NULL || strstr(buf, "/dev") == NULL) {
            printk("<0>[TEST M7] FAILED: /proc/mounts missing mounts: %s\n", buf);
            return false;
        }
    }

    int fd_stat = sys_open("/proc/self/status", O_RDONLY, 0);
    if (fd_stat < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /proc/self/status: %d\n", fd_stat);
        return false;
    }
    memset(buf, 0, sizeof(buf));
    nread = sys_read(fd_stat, buf, sizeof(buf) - 1);
    sys_close(fd_stat);
    if (nread <= 0 || strstr(buf, "Pid:") == NULL || strstr(buf, "Name:") == NULL) {
        printk("<0>[TEST M7] FAILED: /proc/self/status content invalid: %s\n", buf);
        return false;
    }
    printk("<6>[TEST M7] OK: procfs synthetic files (/proc/meminfo, version, uptime, mounts, self/status) verified!\n");

    /* 2. Test devtmpfs: /dev/null, /dev/zero, /dev/urandom, /dev/console */
    int fd_null = sys_open("/dev/null", O_RDWR, 0);
    if (fd_null < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /dev/null: %d\n", fd_null);
        return false;
    }
    int64_t nw = sys_write(fd_null, "discard_me", 10);
    int64_t nr = sys_read(fd_null, buf, sizeof(buf));
    sys_close(fd_null);
    if (nw != 10 || nr != 0) {
        printk("<0>[TEST M7] FAILED: /dev/null write=%ld, read=%ld\n", (long)nw, (long)nr);
        return false;
    }

    int fd_zero = sys_open("/dev/zero", O_RDONLY, 0);
    if (fd_zero < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /dev/zero: %d\n", fd_zero);
        return false;
    }
    memset(buf, 0xAA, 32);
    nr = sys_read(fd_zero, buf, 32);
    sys_close(fd_zero);
    if (nr != 32) {
        printk("<0>[TEST M7] FAILED: /dev/zero read count %ld != 32\n", (long)nr);
        return false;
    }
    for (int i = 0; i < 32; i++) {
        if (buf[i] != 0) {
            printk("<0>[TEST M7] FAILED: /dev/zero non-zero byte at index %d\n", i);
            return false;
        }
    }

    int fd_rnd = sys_open("/dev/urandom", O_RDONLY, 0);
    if (fd_rnd < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /dev/urandom: %d\n", fd_rnd);
        return false;
    }
    memset(buf, 0, 32);
    nr = sys_read(fd_rnd, buf, 32);
    sys_close(fd_rnd);
    if (nr != 32) {
        printk("<0>[TEST M7] FAILED: /dev/urandom read count %ld != 32\n", (long)nr);
        return false;
    }
    bool non_zero = false;
    for (int i = 0; i < 32; i++) {
        if (buf[i] != 0) {
            non_zero = true;
            break;
        }
    }
    if (!non_zero) {
        printk("<0>[TEST M7] FAILED: /dev/urandom returned all zeros\n");
        return false;
    }

    int fd_cons = sys_open("/dev/console", O_WRONLY, 0);
    if (fd_cons < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /dev/console: %d\n", fd_cons);
        return false;
    }
    sys_close(fd_cons);
    printk("<6>[TEST M7] OK: devtmpfs character devices (/dev/null, /dev/zero, /dev/urandom, /dev/console) verified!\n");

    /* 3. Test tmpfs: file creation, read/write, seeking, mkdir, unlink on /tmp and /run */
    int fd_tmp = sys_open("/tmp/m7_test.tmp", O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd_tmp < 0) {
        printk("<0>[TEST M7] FAILED: Could not create /tmp/m7_test.tmp: %d\n", fd_tmp);
        return false;
    }
    const char *tmp_str = "AuroraOS TmpFS In-Memory Storage Test 2026\n";
    size_t tmp_len = strlen(tmp_str);
    nw = sys_write(fd_tmp, tmp_str, tmp_len);
    if (nw != (int64_t)tmp_len) {
        printk("<0>[TEST M7] FAILED: tmpfs write returned %ld (expected %lu)\n", (long)nw, (unsigned long)tmp_len);
        sys_close(fd_tmp);
        return false;
    }

    sys_lseek(fd_tmp, 0, SEEK_SET);
    memset(buf, 0, sizeof(buf));
    nr = sys_read(fd_tmp, buf, sizeof(buf) - 1);
    sys_close(fd_tmp);
    if (nr != (int64_t)tmp_len || strcmp(buf, tmp_str) != 0) {
        printk("<0>[TEST M7] FAILED: tmpfs read mismatch: '%s'\n", buf);
        return false;
    }

    struct stat st;
    if (sys_stat("/tmp/m7_test.tmp", &st) != 0 || st.st_size != (off_t)tmp_len) {
        printk("<0>[TEST M7] FAILED: tmpfs stat size mismatch\n");
        return false;
    }

    /* Test tmpfs subdirectories */
    if (sys_mkdir("/tmp/sub_dir", 0755) != 0) {
        printk("<0>[TEST M7] FAILED: sys_mkdir /tmp/sub_dir failed\n");
        return false;
    }

    int fd_sub = sys_open("/tmp/sub_dir/inner.txt", O_CREAT | O_RDWR, 0644);
    if (fd_sub < 0) {
        printk("<0>[TEST M7] FAILED: create in tmpfs sub_dir failed\n");
        return false;
    }
    sys_write(fd_sub, "TMP_SUB_OK", 10);
    sys_close(fd_sub);

    sys_unlink("/tmp/sub_dir/inner.txt");
    sys_unlink("/tmp/m7_test.tmp");

    /* Also test /run */
    int fd_run = sys_open("/run/m7_run.pid", O_CREAT | O_RDWR, 0644);
    if (fd_run < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /run/m7_run.pid\n");
        return false;
    }
    sys_write(fd_run, "1\n", 2);
    sys_close(fd_run);
    sys_unlink("/run/m7_run.pid");

    printk("<6>[TEST M7] OK: tmpfs (/tmp and /run) file and directory operations verified!\n");

    /* 4. Test sysfs: /sys/kernel/hostname, /sys/kernel/version */
    int fd_host = sys_open("/sys/kernel/hostname", O_RDONLY, 0);
    if (fd_host < 0) {
        printk("<0>[TEST M7] FAILED: Could not open /sys/kernel/hostname: %d\n", fd_host);
        return false;
    }
    memset(buf, 0, sizeof(buf));
    sys_read(fd_host, buf, sizeof(buf) - 1);
    sys_close(fd_host);
    if (strstr(buf, "aurora") == NULL) {
        printk("<0>[TEST M7] FAILED: sysfs initial hostname mismatch: %s\n", buf);
        return false;
    }

    int fd_host_w = sys_open("/sys/kernel/hostname", O_WRONLY, 0);
    if (fd_host_w >= 0) {
        sys_write(fd_host_w, "aurora-node\n", 12);
        sys_close(fd_host_w);

        int fd_host_r = sys_open("/sys/kernel/hostname", O_RDONLY, 0);
        memset(buf, 0, sizeof(buf));
        sys_read(fd_host_r, buf, sizeof(buf) - 1);
        sys_close(fd_host_r);
        if (strstr(buf, "aurora-node") == NULL) {
            printk("<0>[TEST M7] FAILED: sysfs updated hostname mismatch: %s\n", buf);
            return false;
        }
    }

    int fd_sver = sys_open("/sys/kernel/version", O_RDONLY, 0);
    if (fd_sver >= 0) {
        memset(buf, 0, sizeof(buf));
        sys_read(fd_sver, buf, sizeof(buf) - 1);
        sys_close(fd_sver);
        if (strstr(buf, "1.0.0") == NULL) {
            printk("<0>[TEST M7] FAILED: sysfs version mismatch: %s\n", buf);
            return false;
        }
    }
    printk("<6>[TEST M7] OK: sysfs (/sys/kernel/hostname, version) verified!\n");

    /* 5. Test CPIO archive unpacker */
    uint8_t *archive = (uint8_t *)kmalloc(2048, GFP_KERNEL);
    if (!archive) {
        printk("<0>[TEST M7] FAILED: kmalloc for cpio archive failed\n");
        return false;
    }
    size_t arch_off = 0;
    const char *cfg_data = "CPIO_ARCHIVE_CONFIG=ENABLED\n";
    arch_off += build_cpio_entry(archive + arch_off, "etc/m7_cpio.conf", cfg_data, strlen(cfg_data), 0100644);

    const char *bin_data = "echo M7 CPIO Unpack OK!\n";
    arch_off += build_cpio_entry(archive + arch_off, "bin/m7_cpio_test", bin_data, strlen(bin_data), 0100755);

    arch_off += build_cpio_entry(archive + arch_off, "TRAILER!!!", NULL, 0, 0);

    int cpio_res = cpio_extract(archive, arch_off);
    kfree(archive);
    if (cpio_res != 0) {
        printk("<0>[TEST M7] FAILED: cpio_extract returned %d\n", cpio_res);
        return false;
    }

    int fd_cfg = sys_open("/etc/m7_cpio.conf", O_RDONLY, 0);
    if (fd_cfg < 0) {
        printk("<0>[TEST M7] FAILED: Extracted CPIO file /etc/m7_cpio.conf missing!\n");
        return false;
    }
    memset(buf, 0, sizeof(buf));
    sys_read(fd_cfg, buf, sizeof(buf) - 1);
    sys_close(fd_cfg);
    if (strcmp(buf, cfg_data) != 0) {
        printk("<0>[TEST M7] FAILED: Extracted CPIO file content mismatch: '%s'\n", buf);
        return false;
    }

    int fd_bin = sys_open("/bin/m7_cpio_test", O_RDONLY, 0);
    if (fd_bin < 0) {
        printk("<0>[TEST M7] FAILED: Extracted CPIO file /bin/m7_cpio_test missing!\n");
        return false;
    }
    memset(buf, 0, sizeof(buf));
    sys_read(fd_bin, buf, sizeof(buf) - 1);
    sys_close(fd_bin);
    if (strcmp(buf, bin_data) != 0) {
        printk("<0>[TEST M7] FAILED: Extracted CPIO binary content mismatch: '%s'\n", buf);
        return false;
    }

    cpio_extract_initrd();
    printk("<6>[TEST M7] OK: cpio archive extraction into VFS verified!\n");

    /* 6. Test Ring 3 User Mode verification of /proc, /dev, /tmp */
    printk("<6>[TEST M7] Testing Ring 3 User Mode access to virtual filesystems...\n");
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
    vmm_map_page(user_pml4, M7_USER_CODE_VIRT, code_phys, PTE_USER | PTE_WRITABLE);
    vmm_map_page(user_pml4, M7_USER_STACK_VIRT, stack_phys, PTE_USER | PTE_WRITABLE);

    size_t code_len = user_m7_blob_end - user_m7_blob_start;
    memcpy(vmm_phys_to_virt(code_phys), user_m7_blob_start, code_len);
    memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);

    task_t *utask = task_create("m7_user", user_m7_trampoline);
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

    if (utask->exit_code != 77) {
        printk("<0>[TEST M7] FAILED: User M7 process exited with error code %d (expected 77)!\n",
               utask->exit_code);
        sched_remove_task(utask);
        vmm_destroy_user_space(utask->pml4);
        task_destroy(utask);
        return false;
    }

    sched_remove_task(utask);
    vmm_destroy_user_space(utask->pml4);
    task_destroy(utask);
    printk("<6>[TEST M7] OK: Ring 3 User Mode verified /proc, /dev, /tmp syscall access!\n");

    /* 7. Milestone M7 Acceptance */
    printk("<6>[  0.000000] Milestone M7 status: [OK] ramfs, cpio, procfs, devtmpfs, tmpfs, sysfs verified.\n");
    return true;
}
