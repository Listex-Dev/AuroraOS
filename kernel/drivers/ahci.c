#include <aurora/drivers/ahci.h>
#include <aurora/drivers/pci.h>
#include <aurora/drivers/block.h>
#include <aurora/drivers/ata.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>

#define AHCI_MAX_BOUNCE_SECTORS 128
#define AHCI_BOUNCE_SIZE        (AHCI_MAX_BOUNCE_SECTORS * 512) /* 64 KB = 16 pages */

struct ahci_port_dev {
    hba_port_t *port;
    int port_no;
    uintptr_t mem_phys;
    void *mem_virt;
    hba_cmd_header_t *cmd_headers;
    hba_cmd_tbl_t *cmd_tbl;
    uintptr_t cmd_tbl_phys;
    uintptr_t bounce_phys;
    void *bounce_virt;
    uint64_t total_sectors;
    char model[41];
    struct block_device *bdev;
};

static void ahci_port_stop(hba_port_t *port)
{
    port->cmd &= ~(1U << 0); /* Clear ST */
    port->cmd &= ~(1U << 4); /* Clear FRE */

    int spin = 100000;
    while (spin--) {
        if (!(port->cmd & (1U << 15)) && !(port->cmd & (1U << 14))) {
            break;
        }
    }
}

static void ahci_port_start(hba_port_t *port)
{
    int spin = 100000;
    while ((port->cmd & (1U << 15)) && spin--) {
    }

    port->cmd |= (1U << 4); /* FRE */
    port->cmd |= (1U << 0); /* ST */
}

static int ahci_exec_cmd(struct ahci_port_dev *adev, uint8_t command, uint64_t lba, uint32_t count, bool is_write)
{
    hba_port_t *port = adev->port;

    /* Wait until drive is ready */
    int spin = 500000;
    while ((port->tfd & (0x80 | 0x08)) && spin--) {
    }
    if (port->tfd & (0x80 | 0x08)) {
        printk("<3>[AHCI] Port %d busy timeout (tfd=0x%x) before cmd 0x%02x\n",
               adev->port_no, port->tfd, command);
        return -EBUSY;
    }

    /* Clear command table */
    memset(adev->cmd_tbl, 0, sizeof(hba_cmd_tbl_t));
    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)adev->cmd_tbl->cfis;
    fis->fis_type = 0x27; /* Host to Device FIS */
    fis->c = 1;           /* 1 = Command */
    fis->command = command;

    if (command == AHCI_CMD_IDENTIFY) {
        fis->device = 0;
    } else {
        fis->device = 1 << 6; /* LBA mode */
        fis->lba0 = (uint8_t)(lba & 0xFF);
        fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
        fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
        fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
        fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
        fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
        fis->countl = (uint8_t)(count & 0xFF);
        fis->counth = (uint8_t)((count >> 8) & 0xFF);
    }

    uint32_t byte_count = (command == AHCI_CMD_IDENTIFY) ? 512 : (count * 512);
    adev->cmd_tbl->prdt_entry[0].dba = (uint32_t)(adev->bounce_phys & 0xFFFFFFFF);
    adev->cmd_tbl->prdt_entry[0].dbau = (uint32_t)(adev->bounce_phys >> 32);
    adev->cmd_tbl->prdt_entry[0].dbc = (byte_count - 1) & 0x3FFFFF;
    adev->cmd_tbl->prdt_entry[0].i = 1;

    /* Setup Command Header 0 */
    hba_cmd_header_t *hdr = &adev->cmd_headers[0];
    hdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t); /* 5 DWORDS */
    hdr->w = is_write ? 1 : 0;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    __asm__ __volatile__("" ::: "memory");
    port->serr = port->serr;
    port->is = port->is;

    /* Issue command on slot 0 */
    port->ci = 1U;

    /* Poll for completion */
    spin = 1000000;
    while ((port->ci & 1U) && spin--) {
        if (port->tfd & 0x01) {
            printk("<3>[AHCI] Error in tfd (0x%x) during cmd 0x%02x on port %d\n",
                   port->tfd, command, adev->port_no);
            return -EIO;
        }
    }

    if (port->ci & 1U) {
        printk("<3>[AHCI] Command 0x%02x timed out: ci=0x%x, tfd=0x%x\n",
               command, port->ci, port->tfd);
        return -ETIMEDOUT;
    }

    if (port->tfd & 0x01) {
        return -EIO;
    }

    return 0;
}

static int ahci_identify_device(struct ahci_port_dev *adev)
{
    int err = ahci_exec_cmd(adev, AHCI_CMD_IDENTIFY, 0, 1, false);
    if (err != 0) {
        printk("<4>[AHCI] IDENTIFY failed on port %d: %d\n", adev->port_no, err);
        return err;
    }

    uint16_t *id_buf = (uint16_t *)adev->bounce_virt;

    bool lba48 = (id_buf[83] & (1 << 10)) != 0;
    uint64_t total_sectors = 0;
    if (lba48) {
        total_sectors = ((uint64_t)id_buf[103] << 48) |
                        ((uint64_t)id_buf[102] << 32) |
                        ((uint64_t)id_buf[101] << 16) |
                        (uint64_t)id_buf[100];
    }
    if (total_sectors == 0) {
        total_sectors = ((uint32_t)id_buf[61] << 16) | (uint32_t)id_buf[60];
    }
    if (total_sectors == 0) {
        total_sectors = 131072; /* 64 MB fallback */
    }
    adev->total_sectors = total_sectors;

    for (int i = 0; i < 20; i++) {
        adev->model[i * 2] = (char)(id_buf[27 + i] >> 8);
        adev->model[i * 2 + 1] = (char)(id_buf[27 + i] & 0xFF);
    }
    adev->model[40] = '\0';
    for (int i = 39; i >= 0 && adev->model[i] == ' '; i--) {
        adev->model[i] = '\0';
    }

    printk("<6>[AHCI] Port %d drive: %s (%llu sectors, %llu MB, LBA48=%d)\n",
           adev->port_no, adev->model, total_sectors,
           (total_sectors * 512) / (1024 * 1024), lba48 ? 1 : 0);

    return 0;
}

static int ahci_bdev_read(struct block_device *bdev, uint64_t lba, uint32_t count, void *buffer)
{
    struct ahci_port_dev *adev = (struct ahci_port_dev *)bdev->private_data;
    if (!adev) return -EINVAL;

    uint8_t *dst = (uint8_t *)buffer;
    while (count > 0) {
        uint32_t chunk = (count > AHCI_MAX_BOUNCE_SECTORS) ? AHCI_MAX_BOUNCE_SECTORS : count;
        int err = ahci_exec_cmd(adev, AHCI_CMD_READ_DMA_EXT, lba, chunk, false);
        if (err != 0) {
            return err;
        }
        memcpy(dst, adev->bounce_virt, chunk * 512);
        lba += chunk;
        dst += chunk * 512;
        count -= chunk;
    }

    return 0;
}

static int ahci_bdev_write(struct block_device *bdev, uint64_t lba, uint32_t count, const void *buffer)
{
    struct ahci_port_dev *adev = (struct ahci_port_dev *)bdev->private_data;
    if (!adev) return -EINVAL;

    const uint8_t *src = (const uint8_t *)buffer;
    while (count > 0) {
        uint32_t chunk = (count > AHCI_MAX_BOUNCE_SECTORS) ? AHCI_MAX_BOUNCE_SECTORS : count;
        memcpy(adev->bounce_virt, src, chunk * 512);
        int err = ahci_exec_cmd(adev, AHCI_CMD_WRITE_DMA_EXT, lba, chunk, true);
        if (err != 0) {
            return err;
        }
        lba += chunk;
        src += chunk * 512;
        count -= chunk;
    }

    return 0;
}

static int ahci_bdev_flush(struct block_device *bdev)
{
    (void)bdev;
    return 0;
}

static struct block_device_ops g_ahci_bdev_ops = {
    .read_blocks = ahci_bdev_read,
    .write_blocks = ahci_bdev_write,
    .flush = ahci_bdev_flush,
};

int ahci_init(void)
{
    static bool s_initialized = false;
    if (s_initialized) {
        return 0;
    }

    /* Ensure PCI bus scan is complete */
    pci_init();

    struct pci_device *pdev = NULL;
    for (struct pci_device *d = pci_get_device_list(); d != NULL; d = d->next) {
        if (d->class_code == PCI_CLASS_STORAGE && d->subclass == PCI_SUBCLASS_SATA) {
            pdev = d;
            break;
        }
    }

    if (!pdev) {
        printk("<6>[AHCI] No SATA/AHCI controller detected on PCI bus\n");
        return -ENODEV;
    }

    /* Enable Memory Space (bit 1) and Bus Mastering (bit 2) */
    uint16_t cmd = pci_read_config16(pdev->bus, pdev->slot, pdev->func, 0x04);
    cmd |= (1 << 1) | (1 << 2);
    pci_write_config16(pdev->bus, pdev->slot, pdev->func, 0x04, cmd);

    uint64_t abar_phys = pdev->bars[5].base;
    if (!abar_phys) {
        printk("<3>[AHCI] Invalid ABAR base (BAR5 is 0)\n");
        return -ENODEV;
    }

    hba_mem_t *hba = (hba_mem_t *)vmm_map_mmio(abar_phys, 0x2000);
    if (!hba) {
        printk("<3>[AHCI] Failed to map ABAR MMIO at 0x%llx\n", abar_phys);
        return -ENOMEM;
    }

    /* Enable AHCI Mode (GHC bit 31) */
    hba->ghc |= (1U << 31);

    uint32_t pi = hba->pi;
    int drives_found = 0;

    for (int i = 0; i < 32; i++) {
        if (!(pi & (1U << i))) {
            continue;
        }

        hba_port_t *port = &hba->ports[i];
        uint32_t ssts = port->ssts;
        uint8_t det = ssts & 0x0F;
        uint8_t ipm = (ssts >> 8) & 0x0F;

        if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE) {
            continue;
        }

        if (port->sig != SATA_SIG_ATA) {
            continue;
        }

        /* Found SATA disk */
        struct ahci_port_dev *adev = (struct ahci_port_dev *)kzalloc(sizeof(struct ahci_port_dev), GFP_KERNEL);
        if (!adev) {
            continue;
        }

        adev->port = port;
        adev->port_no = i;

        uintptr_t pmem_phys = pmm_alloc_page();
        if (!pmem_phys) {
            kfree(adev);
            continue;
        }
        adev->mem_phys = pmem_phys;
        adev->mem_virt = vmm_phys_to_virt(pmem_phys);
        memset(adev->mem_virt, 0, PAGE_SIZE);

        adev->cmd_headers = (hba_cmd_header_t *)adev->mem_virt;
        adev->cmd_tbl = (hba_cmd_tbl_t *)((uintptr_t)adev->mem_virt + 0x800);
        adev->cmd_tbl_phys = pmem_phys + 0x800;

        adev->cmd_headers[0].ctba = (uint32_t)(adev->cmd_tbl_phys & 0xFFFFFFFF);
        adev->cmd_headers[0].ctbau = (uint32_t)(adev->cmd_tbl_phys >> 32);

        adev->bounce_phys = pmm_alloc_pages(16); /* 64 KB */
        if (!adev->bounce_phys) {
            pmm_free_page(pmem_phys);
            kfree(adev);
            continue;
        }
        adev->bounce_virt = vmm_phys_to_virt(adev->bounce_phys);
        memset(adev->bounce_virt, 0, AHCI_BOUNCE_SIZE);

        /* Configure and start port */
        ahci_port_stop(port);
        port->clb = (uint32_t)(pmem_phys & 0xFFFFFFFF);
        port->clbu = (uint32_t)(pmem_phys >> 32);
        port->fb = (uint32_t)((pmem_phys + 0x400) & 0xFFFFFFFF);
        port->fbu = (uint32_t)((pmem_phys + 0x400) >> 32);
        port->serr = port->serr;
        port->is = port->is;
        ahci_port_start(port);

        int id_err = ahci_identify_device(adev);
        if (id_err != 0) {
            adev->total_sectors = 131072; /* 64 MB fallback */
        }

        struct block_device *sda = (struct block_device *)kzalloc(sizeof(struct block_device), GFP_KERNEL);
        if (!sda) {
            continue;
        }

        snprintf(sda->name, sizeof(sda->name), "sd%c", 'a' + drives_found);
        sda->dev_id = (8 << 8) | (drives_found * 16);
        sda->block_size = 512;
        sda->total_blocks = adev->total_sectors;
        sda->ops = &g_ahci_bdev_ops;
        sda->private_data = adev;
        adev->bdev = sda;

        bdev_register(sda);
        ata_scan_partitions(sda);

        drives_found++;
        if (drives_found == 1) {
            break;
        }
    }

    s_initialized = true;
    return (drives_found > 0) ? 0 : -ENODEV;
}
