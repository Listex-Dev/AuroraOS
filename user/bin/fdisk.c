#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

struct mbr_partition {
    uint8_t  bootable;
    uint8_t  chs_start[3];
    uint8_t  type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t num_sectors;
} __attribute__((packed));

struct gpt_header {
    uint64_t signature; /* "EFI PART" */
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t sizeof_partition_entry;
    uint32_t partition_entry_array_crc32;
} __attribute__((packed));

struct gpt_partition_entry {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    uint16_t name[36];
} __attribute__((packed));

static const char *get_mbr_type_name(uint8_t type)
{
    switch (type) {
        case 0x83: return "Linux";
        case 0x82: return "Linux swap / Solaris";
        case 0x07: return "HPFS/NTFS/exFAT";
        case 0x0b: return "W95 FAT32";
        case 0x0c: return "W95 FAT32 (LBA)";
        case 0x05: return "Extended";
        case 0x0f: return "W95 Ext'd (LBA)";
        case 0xee: return "GPT";
        case 0xef: return "EFI (FAT-12/16/32)";
        default:   return "Linux / Other";
    }
}

static void format_size(uint64_t bytes, char *out, size_t out_sz)
{
    if (bytes >= (1024ULL * 1024 * 1024)) {
        snprintf(out, out_sz, "%lluG", bytes / (1024ULL * 1024 * 1024));
    } else if (bytes >= (1024ULL * 1024)) {
        snprintf(out, out_sz, "%lluM", bytes / (1024ULL * 1024));
    } else if (bytes >= 1024ULL) {
        snprintf(out, out_sz, "%lluK", bytes / 1024ULL);
    } else {
        snprintf(out, out_sz, "%lluB", bytes);
    }
}

static int list_device(const char *devpath)
{
    int fd = open(devpath, O_RDONLY);
    if (fd < 0) {
        perror(devpath);
        return 1;
    }

    off_t disk_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (disk_size <= 0) {
        /* Fallback if block device seeking returns 0 or unsupported */
        disk_size = 64 * 1024 * 1024;
    }

    uint64_t total_sectors = disk_size / 512;
    uint64_t size_mib = disk_size / (1024 * 1024);

    printf("Disk %s: %llu MiB, %lld bytes, %llu sectors\n",
           devpath, size_mib, (long long)disk_size, total_sectors);
    printf("Units: sectors of 1 * 512 = 512 bytes\n");
    printf("Sector size (logical/physical): 512 bytes / 512 bytes\n");
    printf("I/O size (minimum/optimal): 512 bytes / 512 bytes\n");

    uint8_t sector0[512];
    ssize_t n = read(fd, sector0, sizeof(sector0));
    if (n < 512) {
        fprintf(stderr, "%s: unable to read first sector\n", devpath);
        close(fd);
        return 1;
    }

    if (sector0[510] != 0x55 || sector0[511] != 0xAA) {
        printf("Disklabel type: unknown (no MBR signature 0xAA55)\n\n");
        close(fd);
        return 0;
    }

    struct mbr_partition *parts = (struct mbr_partition *)(sector0 + 446);
    bool is_gpt = false;
    for (int i = 0; i < 4; i++) {
        if (parts[i].type == 0xEE) {
            is_gpt = true;
            break;
        }
    }

    if (is_gpt) {
        printf("Disklabel type: gpt\n\n");
        printf("%-15s %8s %8s %8s %5s %s\n",
               "Device", "Start", "End", "Sectors", "Size", "Type");

        uint8_t gpt_sec[512];
        if (read(fd, gpt_sec, sizeof(gpt_sec)) == 512) {
            struct gpt_header *gh = (struct gpt_header *)gpt_sec;
            if (gh->signature == 0x5452415020494645ULL) {
                uint32_t num_entries = gh->num_partition_entries;
                if (num_entries > 128) num_entries = 128;
                uint8_t entry_buf[512];
                lseek(fd, gh->partition_entry_lba * 512, SEEK_SET);

                int part_idx = 0;
                for (uint32_t e = 0; e < num_entries; e++) {
                    uint32_t in_sector = (e * sizeof(struct gpt_partition_entry)) % 512;
                    if (in_sector == 0) {
                        if (read(fd, entry_buf, 512) < 512) break;
                    }
                    struct gpt_partition_entry *ge = (struct gpt_partition_entry *)(entry_buf + in_sector);
                    bool non_zero = false;
                    for (int g = 0; g < 16; g++) {
                        if (ge->type_guid[g] != 0) {
                            non_zero = true;
                            break;
                        }
                    }
                    if (!non_zero || ge->ending_lba < ge->starting_lba) continue;

                    part_idx++;
                    char devname[64];
                    snprintf(devname, sizeof(devname), "%s%d", devpath, part_idx);
                    uint64_t sectors = ge->ending_lba - ge->starting_lba + 1;
                    char sz_str[16];
                    format_size(sectors * 512, sz_str, sizeof(sz_str));

                    printf("%-15s %8llu %8llu %8llu %5s Linux filesystem\n",
                           devname, ge->starting_lba, ge->ending_lba, sectors, sz_str);
                }
            }
        }
    } else {
        printf("Disklabel type: dos\n\n");
        printf("%-15s %4s %8s %8s %8s %5s %2s %s\n",
               "Device", "Boot", "Start", "End", "Sectors", "Size", "Id", "Type");

        for (int i = 0; i < 4; i++) {
            if (parts[i].type == 0 || parts[i].num_sectors == 0) {
                continue;
            }

            char devname[64];
            snprintf(devname, sizeof(devname), "%s%d", devpath, i + 1);

            uint32_t start = parts[i].lba_start;
            uint32_t sectors = parts[i].num_sectors;
            uint32_t end = start + sectors - 1;

            char sz_str[16];
            format_size((uint64_t)sectors * 512, sz_str, sizeof(sz_str));

            const char *boot = (parts[i].bootable & 0x80) ? "*" : "";

            printf("%-15s %4s %8u %8u %8u %5s %02x %s\n",
                   devname, boot, start, end, sectors, sz_str,
                   parts[i].type, get_mbr_type_name(parts[i].type));
        }
    }

    printf("\n");
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    int opt_l = 0;
    const char *target = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            opt_l = 1;
        } else if (argv[i][0] != '-') {
            target = argv[i];
        }
    }

    if (!opt_l && !target) {
        fprintf(stderr, "Usage: %s -l [device...]\n", argv[0]);
        return 1;
    }

    if (target) {
        return list_device(target);
    }

    /* If -l with no device: inspect /proc/partitions or /dev/sda */
    int ret = 0;
    struct stat st;
    if (stat("/dev/sda", &st) == 0) {
        ret |= list_device("/dev/sda");
    }
    if (stat("/dev/sdb", &st) == 0) {
        ret |= list_device("/dev/sdb");
    }

    return ret;
}
