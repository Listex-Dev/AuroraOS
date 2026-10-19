#include <aurora/drivers/pci.h>
#include <aurora/fs/sysfs.h>
#include <aurora/fs/vfs.h>
#include <aurora/io.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>
#include <aurora/printk.h>

static struct pci_device *g_pci_devices = NULL;

uint32_t pci_read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = (1U << 31) |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t val = pci_read_config32(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t val = pci_read_config32(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val)
{
    uint32_t address = (1U << 31) |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, val);
}

void pci_write_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val)
{
    uint32_t address = (1U << 31) |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outw(PCI_CONFIG_DATA + (offset & 2), val);
}

void pci_write_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val)
{
    uint32_t address = (1U << 31) |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    outb(PCI_CONFIG_DATA + (offset & 3), val);
}

struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    for (struct pci_device *dev = g_pci_devices; dev != NULL; dev = dev->next) {
        if (dev->vendor_id == vendor_id && dev->device_id == device_id) {
            return dev;
        }
    }
    return NULL;
}

struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass)
{
    for (struct pci_device *dev = g_pci_devices; dev != NULL; dev = dev->next) {
        if (dev->class_code == class_code && dev->subclass == subclass) {
            return dev;
        }
    }
    return NULL;
}

struct pci_device *pci_get_device_list(void)
{
    return g_pci_devices;
}

static void pci_probe_bars(struct pci_device *dev)
{
    /* Only probe BARs for standard PCI devices (Header Type 0) */
    if ((dev->header_type & 0x7F) != 0) {
        return;
    }

    for (int i = 0; i < 6; i++) {
        uint8_t bar_off = 0x10 + (i * 4);
        uint32_t orig = pci_read_config32(dev->bus, dev->slot, dev->func, bar_off);

        pci_write_config32(dev->bus, dev->slot, dev->func, bar_off, 0xFFFFFFFF);
        uint32_t mask = pci_read_config32(dev->bus, dev->slot, dev->func, bar_off);
        pci_write_config32(dev->bus, dev->slot, dev->func, bar_off, orig);

        if (mask == 0 || mask == 0xFFFFFFFF) {
            continue;
        }

        if (orig & PCI_BAR_IO) {
            dev->bars[i].is_io = true;
            dev->bars[i].base = orig & ~0x3ULL;
            dev->bars[i].size = (~(mask & ~0x3ULL)) + 1;
        } else {
            dev->bars[i].is_io = false;
            dev->bars[i].prefetch = (orig & PCI_BAR_PREFETCH) != 0;
            if ((orig & PCI_BAR_64) && (i < 5)) {
                dev->bars[i].is_64 = true;
                uint8_t hi_off = bar_off + 4;
                uint32_t orig_hi = pci_read_config32(dev->bus, dev->slot, dev->func, hi_off);
                pci_write_config32(dev->bus, dev->slot, dev->func, hi_off, 0xFFFFFFFF);
                uint32_t mask_hi = pci_read_config32(dev->bus, dev->slot, dev->func, hi_off);
                pci_write_config32(dev->bus, dev->slot, dev->func, hi_off, orig_hi);

                dev->bars[i].base = ((uint64_t)orig_hi << 32) | (orig & ~0xFULL);
                uint64_t full_mask = ((uint64_t)mask_hi << 32) | (mask & ~0xFULL);
                dev->bars[i].size = (~full_mask) + 1;
                i++; /* Skip next 32-bit BAR as it is the high half */
            } else {
                dev->bars[i].base = orig & ~0xFULL;
                dev->bars[i].size = (~(mask & ~0xFULL)) + 1;
            }
        }
    }
}

static void pci_identify_driver(struct pci_device *dev)
{
    if (dev->class_code == PCI_CLASS_STORAGE) {
        if (dev->subclass == PCI_SUBCLASS_IDE) {
            strncpy(dev->driver_name, "ata_piix", sizeof(dev->driver_name) - 1);
        } else if (dev->subclass == PCI_SUBCLASS_SATA) {
            strncpy(dev->driver_name, "ahci", sizeof(dev->driver_name) - 1);
        } else {
            strncpy(dev->driver_name, "storage", sizeof(dev->driver_name) - 1);
        }
    } else if (dev->class_code == PCI_CLASS_NETWORK) {
        if (dev->vendor_id == 0x8086 && dev->device_id == 0x100E) {
            strncpy(dev->driver_name, "e1000", sizeof(dev->driver_name) - 1);
        } else if (dev->vendor_id == 0x1AF4 && dev->device_id == 0x1000) {
            strncpy(dev->driver_name, "virtio-net", sizeof(dev->driver_name) - 1);
        } else {
            strncpy(dev->driver_name, "net", sizeof(dev->driver_name) - 1);
        }
    } else if (dev->class_code == PCI_CLASS_DISPLAY) {
        strncpy(dev->driver_name, "vga", sizeof(dev->driver_name) - 1);
    } else if (dev->class_code == PCI_CLASS_BRIDGE) {
        if (dev->subclass == PCI_SUBCLASS_HOST) {
            strncpy(dev->driver_name, "host_bridge", sizeof(dev->driver_name) - 1);
        } else if (dev->subclass == PCI_SUBCLASS_ISA) {
            strncpy(dev->driver_name, "isa_bridge", sizeof(dev->driver_name) - 1);
        } else {
            strncpy(dev->driver_name, "pci_bridge", sizeof(dev->driver_name) - 1);
        }
    } else {
        strncpy(dev->driver_name, "generic", sizeof(dev->driver_name) - 1);
    }
}

static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint32_t val = pci_read_config32(bus, slot, func, 0x00);
    uint16_t vendor = (uint16_t)(val & 0xFFFF);
    uint16_t device = (uint16_t)((val >> 16) & 0xFFFF);

    if (vendor == 0xFFFF || vendor == 0x0000) {
        return;
    }

    struct pci_device *pdev = (struct pci_device *)kzalloc(sizeof(struct pci_device), GFP_KERNEL);
    if (!pdev) {
        return;
    }

    pdev->bus = bus;
    pdev->slot = slot;
    pdev->func = func;
    pdev->vendor_id = vendor;
    pdev->device_id = device;

    uint32_t class_reg = pci_read_config32(bus, slot, func, 0x08);
    pdev->revision_id = (uint8_t)(class_reg & 0xFF);
    pdev->prog_if = (uint8_t)((class_reg >> 8) & 0xFF);
    pdev->subclass = (uint8_t)((class_reg >> 16) & 0xFF);
    pdev->class_code = (uint8_t)((class_reg >> 24) & 0xFF);

    uint32_t header_reg = pci_read_config32(bus, slot, func, 0x0C);
    pdev->header_type = (uint8_t)((header_reg >> 16) & 0xFF);

    uint32_t irq_reg = pci_read_config32(bus, slot, func, 0x3C);
    pdev->irq_line = (uint8_t)(irq_reg & 0xFF);
    pdev->irq_pin = (uint8_t)((irq_reg >> 8) & 0xFF);

    pci_probe_bars(pdev);
    pci_identify_driver(pdev);

    /* Append to device list */
    pdev->next = g_pci_devices;
    g_pci_devices = pdev;

    printk("<6>[PCI] %02x:%02x.%d [%04x:%04x] class 0x%02x%02x (prog %02x) driver: %s, IRQ %u\n",
           bus, slot, func, vendor, device, pdev->class_code, pdev->subclass,
           pdev->prog_if, pdev->driver_name, pdev->irq_line);

    for (int b = 0; b < 6; b++) {
        if (pdev->bars[b].size > 0) {
            printk("<6>[PCI]   BAR%d: %s 0x%llx (size 0x%llx)\n",
                   b, pdev->bars[b].is_io ? "I/O" : "MEM",
                   pdev->bars[b].base, pdev->bars[b].size);
        }
    }
}

static void pci_scan_device(uint8_t bus, uint8_t slot)
{
    uint32_t val = pci_read_config32(bus, slot, 0, 0x00);
    uint16_t vendor = (uint16_t)(val & 0xFFFF);
    if (vendor == 0xFFFF || vendor == 0x0000) {
        return;
    }

    pci_scan_function(bus, slot, 0);

    uint32_t header_reg = pci_read_config32(bus, slot, 0, 0x0C);
    uint8_t header_type = (uint8_t)((header_reg >> 16) & 0xFF);

    if (header_type & 0x80) {
        /* Multi-function device */
        for (uint8_t func = 1; func < 8; func++) {
            uint32_t fval = pci_read_config32(bus, slot, func, 0x00);
            if ((fval & 0xFFFF) != 0xFFFF && (fval & 0xFFFF) != 0x0000) {
                pci_scan_function(bus, slot, func);
            }
        }
    }
}

void pci_init(void)
{
    printk("<6>[  0.000000] PCI: Initializing PCI bus subsystem and scanning devices...\n");
    g_pci_devices = NULL;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            pci_scan_device((uint8_t)bus, slot);
        }
    }

    int dev_count = 0;
    for (struct pci_device *d = g_pci_devices; d != NULL; d = d->next) {
        dev_count++;
    }

    printk("<6>[  0.000000] PCI: Scan complete, %d PCI devices detected\n", dev_count);
}
