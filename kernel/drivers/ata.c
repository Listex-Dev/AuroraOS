#include <aurora/drivers/ata.h>
#include <aurora/drivers/block.h>
#include <aurora/io.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

#define ATA_REG_DATA        (ATA_PRIMARY_IO + 0)
#define ATA_REG_ERROR       (ATA_PRIMARY_IO + 1)
#define ATA_REG_FEATURES    (ATA_PRIMARY_IO + 1)
#define ATA_REG_SEC_COUNT   (ATA_PRIMARY_IO + 2)
#define ATA_REG_LBA_LOW     (ATA_PRIMARY_IO + 3)
#define ATA_REG_LBA_MID     (ATA_PRIMARY_IO + 4)
#define ATA_REG_LBA_HIGH    (ATA_PRIMARY_IO + 5)
#define ATA_REG_DRIVE_HEAD  (ATA_PRIMARY_IO + 6)
#define ATA_REG_STATUS      (ATA_PRIMARY_IO + 7)
#define ATA_REG_COMMAND     (ATA_PRIMARY_IO + 7)

#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DF           0x20
#define ATA_SR_DRQ          0x08
#define ATA_SR_ERR          0x01

#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_IDENTIFY        0xEC

struct mbr_partition {
    uint8_t  bootable;
    uint8_t  chs_start[3];
    uint8_t  type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t num_sectors;
} __attribute__((packed));

struct partition_info {
    struct block_device *parent;
    uint64_t lba_start;
    uint64_t num_sectors;
};

static void ata_io_delay(void)
{
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
}

static int ata_wait_bsy(void)
{
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
        io_wait();
    }
    return -ETIMEDOUT;
}

static int ata_wait_drq(void)
{
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            return -EIO;
        }
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 0;
        }
        io_wait();
    }
    return -ETIMEDOUT;
}

static int ata_read_sector(uint64_t lba, uint16_t *buf)
{
    if (ata_wait_bsy() != 0) return -ETIMEDOUT;

    if (lba < 0x10000000ULL) {
        /* LBA28 */
        outb(ATA_REG_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_REG_SEC_COUNT, 1);
        outb(ATA_REG_LBA_LOW, (uint8_t)lba);
        outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    } else {
        /* LBA48 */
        outb(ATA_REG_DRIVE_HEAD, 0x40);
        outb(ATA_REG_SEC_COUNT, 0);
        outb(ATA_REG_LBA_LOW, (uint8_t)(lba >> 24));
        outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 32));
        outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 40));
        outb(ATA_REG_SEC_COUNT, 1);
        outb(ATA_REG_LBA_LOW, (uint8_t)lba);
        outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    }

    if (ata_wait_drq() != 0) return -EIO;

    for (int i = 0; i < 256; i++) {
        buf[i] = inw(ATA_REG_DATA);
    }

    ata_io_delay();
    return 0;
}

static int ata_write_sector(uint64_t lba, const uint16_t *buf)
{
    if (ata_wait_bsy() != 0) return -ETIMEDOUT;

    if (lba < 0x10000000ULL) {
        /* LBA28 */
        outb(ATA_REG_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_REG_SEC_COUNT, 1);
        outb(ATA_REG_LBA_LOW, (uint8_t)lba);
        outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    } else {
        /* LBA48 */
        outb(ATA_REG_DRIVE_HEAD, 0x40);
        outb(ATA_REG_SEC_COUNT, 0);
        outb(ATA_REG_LBA_LOW, (uint8_t)(lba >> 24));
        outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 32));
        outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 40));
        outb(ATA_REG_SEC_COUNT, 1);
        outb(ATA_REG_LBA_LOW, (uint8_t)lba);
        outb(ATA_REG_LBA_MID, (uint8_t)(lba >> 8));
        outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
        outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
    }

    if (ata_wait_drq() != 0) return -EIO;

    for (int i = 0; i < 256; i++) {
        outw(ATA_REG_DATA, buf[i]);
    }

    ata_io_delay();
    return ata_wait_bsy();
}

static int ata_bdev_read(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer)
{
    (void)bdev;
    uint8_t *ptr = (uint8_t *)buffer;
    for (uint32_t i = 0; i < count; i++) {
        int err = ata_read_sector(lba + i, (uint16_t *)(ptr + (i * 512)));
        if (err != 0) {
            return err;
        }
    }
    return 0;
}

static int ata_bdev_write(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer)
{
    (void)bdev;
    const uint8_t *ptr = (const uint8_t *)buffer;
    for (uint32_t i = 0; i < count; i++) {
        int err = ata_write_sector(lba + i, (const uint16_t *)(ptr + (i * 512)));
        if (err != 0) {
            return err;
        }
    }
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
    return 0;
}

static int ata_bdev_flush(struct block_device *bdev)
{
    (void)bdev;
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return ata_wait_bsy();
}

static struct block_device_ops g_ata_bdev_ops = {
    .read_blocks = ata_bdev_read,
    .write_blocks = ata_bdev_write,
    .flush = ata_bdev_flush,
};

static int part_bdev_read(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer)
{
    struct partition_info *pi = (struct partition_info *)bdev->private_data;
    if (!pi || !pi->parent) return -EINVAL;
    if (count > pi->num_sectors || lba > pi->num_sectors - count) return -EIO;
    if (lba > UINT64_MAX - pi->lba_start) return -EINVAL;
    return bdev_read(pi->parent, pi->lba_start + lba, count, buffer);
}

static int part_bdev_write(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer)
{
    struct partition_info *pi = (struct partition_info *)bdev->private_data;
    if (!pi || !pi->parent) return -EINVAL;
    if (count > pi->num_sectors || lba > pi->num_sectors - count) return -EIO;
    if (lba > UINT64_MAX - pi->lba_start) return -EINVAL;
    return bdev_write(pi->parent, pi->lba_start + lba, count, buffer);
}

static int part_bdev_flush(struct block_device *bdev)
{
    struct partition_info *pi = (struct partition_info *)bdev->private_data;
    if (!pi || !pi->parent) return -EINVAL;
    return bdev_flush(pi->parent);
}

static struct block_device_ops g_part_bdev_ops = {
    .read_blocks = part_bdev_read,
    .write_blocks = part_bdev_write,
    .flush = part_bdev_flush,
};

struct gpt_header {
    uint64_t signature; /* "EFI PART" = 0x5452415020494645ULL */
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

void ata_scan_partitions(struct block_device *disk_bdev)
{
    uint8_t sector[512];
    if (bdev_read(disk_bdev, 0, 1, sector) != 0) {
        return;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return; /* No MBR signature */
    }

    struct mbr_partition *parts = (struct mbr_partition *)(sector + 446);
    bool has_gpt = false;
    for (int i = 0; i < 4; i++) {
        if (parts[i].type == 0xEE) {
            has_gpt = true;
            break;
        }
    }

    if (has_gpt) {
        uint8_t gpt_buf[512];
        if (bdev_read(disk_bdev, 1, 1, gpt_buf) == 0) {
            struct gpt_header *gh = (struct gpt_header *)gpt_buf;
            if (gh->signature == 0x5452415020494645ULL) {
                uint32_t num_entries = gh->num_partition_entries;
                if (num_entries > 128) num_entries = 128;
                uint64_t entry_lba = gh->partition_entry_lba;
                uint8_t entry_buf[512];
                int part_idx = 0;

                for (uint32_t e = 0; e < num_entries; e++) {
                    uint64_t lba_offset = entry_lba + (e * sizeof(struct gpt_partition_entry)) / 512;
                    uint32_t in_sector_offset = (e * sizeof(struct gpt_partition_entry)) % 512;
                    if (in_sector_offset == 0) {
                        if (bdev_read(disk_bdev, lba_offset, 1, entry_buf) != 0) {
                            break;
                        }
                    }
                    struct gpt_partition_entry *ge = (struct gpt_partition_entry *)(entry_buf + in_sector_offset);
                    bool non_zero = false;
                    for (int g = 0; g < 16; g++) {
                        if (ge->type_guid[g] != 0) {
                            non_zero = true;
                            break;
                        }
                    }
                    if (!non_zero || ge->ending_lba < ge->starting_lba) {
                        continue;
                    }

                    part_idx++;
                    struct partition_info *pi = (struct partition_info *)kzalloc(sizeof(struct partition_info), GFP_KERNEL);
                    if (!pi) continue;
                    pi->parent = disk_bdev;
                    pi->lba_start = ge->starting_lba;
                    pi->num_sectors = ge->ending_lba - ge->starting_lba + 1;

                    struct block_device *part_bdev = (struct block_device *)kzalloc(sizeof(struct block_device), GFP_KERNEL);
                    if (!part_bdev) {
                        kfree(pi);
                        continue;
                    }
                    snprintf(part_bdev->name, sizeof(part_bdev->name), "%s%d", disk_bdev->name, part_idx);
                    part_bdev->dev_id = (disk_bdev->dev_id & 0xFF00) | part_idx;
                    part_bdev->block_size = 512;
                    part_bdev->total_blocks = pi->num_sectors;
                    part_bdev->ops = &g_part_bdev_ops;
                    part_bdev->private_data = pi;

                    bdev_register(part_bdev);
                    printk("<6>[GPT] Registered partition /dev/%s (LBA %llu..%llu, %llu sectors)\n",
                           part_bdev->name, ge->starting_lba, ge->ending_lba, pi->num_sectors);
                }
                return;
            }
        }
    }

    /* Standard MBR partitions */
    for (int i = 0; i < 4; i++) {
        if (parts[i].type == 0 || parts[i].num_sectors == 0) {
            continue;
        }

        struct partition_info *pi = (struct partition_info *)kzalloc(sizeof(struct partition_info), GFP_KERNEL);
        if (!pi) continue;

        pi->parent = disk_bdev;
        pi->lba_start = parts[i].lba_start;
        pi->num_sectors = parts[i].num_sectors;

        struct block_device *part_bdev = (struct block_device *)kzalloc(sizeof(struct block_device), GFP_KERNEL);
        if (!part_bdev) {
            kfree(pi);
            continue;
        }

        snprintf(part_bdev->name, sizeof(part_bdev->name), "%s%d", disk_bdev->name, i + 1);
        part_bdev->dev_id = (disk_bdev->dev_id & 0xFF00) | (i + 1);
        part_bdev->block_size = 512;
        part_bdev->total_blocks = parts[i].num_sectors;
        part_bdev->ops = &g_part_bdev_ops;
        part_bdev->private_data = pi;

        bdev_register(part_bdev);
        printk("<6>[MBR] Registered partition /dev/%s (type 0x%02x, start LBA %u, %u sectors)\n",
               part_bdev->name, parts[i].type, parts[i].lba_start, parts[i].num_sectors);
    }
}

int ata_init(void)
{
    if (bdev_get("sda")) {
        return 0;
    }

    /* Select master drive */
    outb(ATA_REG_DRIVE_HEAD, 0xA0);
    ata_io_delay();

    outb(ATA_REG_SEC_COUNT, 0);
    outb(ATA_REG_LBA_LOW, 0);
    outb(ATA_REG_LBA_MID, 0);
    outb(ATA_REG_LBA_HIGH, 0);
    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_delay();

    uint8_t status = inb(ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) {
        printk("<6>[ATA] No primary master drive detected on 0x1F0\n");
        return -ENODEV;
    }

    if (ata_wait_bsy() != 0) {
        printk("<4>[ATA] Drive busy timeout\n");
        return -ETIMEDOUT;
    }

    uint8_t mid = inb(ATA_REG_LBA_MID);
    uint8_t high = inb(ATA_REG_LBA_HIGH);
    if (mid != 0 || high != 0) {
        printk("<6>[ATA] Non-ATA drive detected (ATAPI)\n");
        return -ENODEV;
    }

    if (ata_wait_drq() != 0) {
        printk("<4>[ATA] DRQ wait timeout on IDENTIFY\n");
        return -EIO;
    }

    uint16_t id_buf[256];
    for (int i = 0; i < 256; i++) {
        id_buf[i] = inw(ATA_REG_DATA);
    }

    uint64_t total_sectors = 0;
    bool lba48 = (id_buf[83] & (1 << 10)) != 0;
    if (lba48) {
        total_sectors = ((uint64_t)id_buf[103] << 48) |
                        ((uint64_t)id_buf[102] << 32) |
                        ((uint64_t)id_buf[101] << 16) |
                        (uint64_t)id_buf[100];
    }
    if (total_sectors == 0) {
        total_sectors = ((uint32_t)id_buf[61] << 16) | (uint32_t)id_buf[60];
    }

    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2] = (char)(id_buf[27 + i] >> 8);
        model[i * 2 + 1] = (char)(id_buf[27 + i] & 0xFF);
    }
    model[40] = '\0';
    for (int i = 39; i >= 0 && model[i] == ' '; i--) {
        model[i] = '\0';
    }

    printk("<6>[ATA] Primary Master: %s (%llu sectors, %llu MB, LBA48=%d)\n",
           model, total_sectors, (total_sectors * 512) / (1024 * 1024), lba48 ? 1 : 0);

    struct block_device *sda = (struct block_device *)kzalloc(sizeof(struct block_device), GFP_KERNEL);
    if (!sda) return -ENOMEM;

    strncpy(sda->name, "sda", sizeof(sda->name) - 1);
    sda->dev_id = (8 << 8) | 0; /* major 8, minor 0 */
    sda->block_size = 512;
    sda->total_blocks = total_sectors;
    sda->ops = &g_ata_bdev_ops;

    bdev_register(sda);
    ata_scan_partitions(sda);

    return 0;
}
