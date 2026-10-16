#include <aurora/fs/cpio.h>
#include <aurora/fs/vfs.h>
#include <aurora/bootinfo.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

#define KERNEL_DIRECT_MAP_BASE 0xFFFF800000000000ULL

static uint32_t parse_hex8(const char *s)
{
    uint32_t val = 0;
    for (int i = 0; i < 8; i++) {
        char c = s[i];
        uint32_t digit = 0;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            break;
        }
        val = (val << 4) | digit;
    }
    return val;
}

static void ensure_parent_dirs(const char *path)
{
    char tmp[256];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return;
    memcpy(tmp, path, len + 1);

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            vfs_mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
}

int cpio_extract(const void *archive, size_t size)
{
    if (!archive || size < sizeof(struct cpio_newc_header)) {
        return -EINVAL;
    }

    const uint8_t *ptr = (const uint8_t *)archive;
    const uint8_t *end = ptr + size;
    int files_extracted = 0;

    while (ptr + sizeof(struct cpio_newc_header) <= end) {
        const struct cpio_newc_header *hdr = (const struct cpio_newc_header *)ptr;

        if (strncmp(hdr->c_magic, "070701", 6) != 0 &&
            strncmp(hdr->c_magic, "070702", 6) != 0) {
            printk("<3>[CPIO] Invalid magic: %.6s\n", hdr->c_magic);
            return -EINVAL;
        }

        uint32_t mode = parse_hex8(hdr->c_mode);
        uint32_t filesize = parse_hex8(hdr->c_filesize);
        uint32_t namesize = parse_hex8(hdr->c_namesize);

        if (namesize == 0 || namesize > 1024) {
            return -EINVAL;
        }

        const char *raw_name = (const char *)(ptr + sizeof(struct cpio_newc_header));
        if (strcmp(raw_name, "TRAILER!!!") == 0) {
            /* End of archive */
            break;
        }

        size_t name_padded = ((sizeof(struct cpio_newc_header) + namesize + 3) & ~3);
        const uint8_t *data_ptr = ptr + name_padded;
        size_t data_padded = ((filesize + 3) & ~3);

        if (data_ptr + filesize > end) {
            printk("<3>[CPIO] File data exceeds archive bounds\n");
            return -EINVAL;
        }

        /* Build absolute path */
        char path[256];
        const char *p = raw_name;
        if (p[0] == '.' && p[1] == '/') {
            p += 2;
        }
        while (*p == '/') {
            p++;
        }

        if (strlen(p) == 0 || strcmp(p, ".") == 0) {
            ptr = data_ptr + data_padded;
            continue;
        }

        snprintf(path, sizeof(path), "/%s", p);

        uint32_t ftype = mode & 0170000;
        if (ftype == 0040000) {
            /* Directory */
            ensure_parent_dirs(path);
            vfs_mkdir(path, mode & 0777);
        } else if (ftype == 0100000 || ftype == 0) {
            /* Regular file */
            ensure_parent_dirs(path);
            struct file *f = NULL;
            int err = vfs_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode & 0777, &f);
            if (err == 0 && f) {
                if (filesize > 0) {
                    vfs_write(f, data_ptr, filesize);
                }
                vfs_close(f);
                files_extracted++;
            } else {
                printk("<3>[CPIO] Failed to create file: %s (err=%d)\n", path, err);
            }
        }

        ptr = data_ptr + data_padded;
    }

    printk("<6>[CPIO] Extracted %d files successfully\n", files_extracted);
    return 0;
}

int cpio_extract_initrd(void)
{
    if (g_boot_info.initrd_start == 0 || g_boot_info.initrd_end <= g_boot_info.initrd_start) {
        return 0; /* No initrd loaded */
    }

    size_t size = g_boot_info.initrd_end - g_boot_info.initrd_start;
    const void *archive = (const void *)(KERNEL_DIRECT_MAP_BASE + g_boot_info.initrd_start);

    printk("<6>[CPIO] Extracting initrd module at phys 0x%llx..0x%llx (%u bytes)...\n",
           g_boot_info.initrd_start, g_boot_info.initrd_end, (uint32_t)size);

    return cpio_extract(archive, size);
}
