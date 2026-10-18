#ifndef _AURORA_DRIVERS_PCI_H
#define _AURORA_DRIVERS_PCI_H

#include <aurora/types.h>

#define PCI_CONFIG_ADDR   0x0CF8
#define PCI_CONFIG_DATA   0x0CFC

#define PCI_BAR_IO        0x01
#define PCI_BAR_64        0x04
#define PCI_BAR_PREFETCH  0x08

/* PCI Class Codes */
#define PCI_CLASS_STORAGE   0x01
#define PCI_SUBCLASS_IDE    0x01
#define PCI_SUBCLASS_SATA   0x06
#define PCI_PROGIF_AHCI     0x01

#define PCI_CLASS_NETWORK   0x02
#define PCI_SUBCLASS_ETHER  0x00

#define PCI_CLASS_DISPLAY   0x03
#define PCI_SUBCLASS_VGA    0x00

#define PCI_CLASS_BRIDGE    0x06
#define PCI_SUBCLASS_HOST   0x00
#define PCI_SUBCLASS_ISA    0x01
#define PCI_SUBCLASS_PCI    0x04

struct pci_bar {
    uint64_t base;
    uint64_t size;
    bool is_io;
    bool is_64;
    bool prefetch;
};

struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision_id;
    uint8_t  header_type;
    uint8_t  irq_line;
    uint8_t  irq_pin;
    struct pci_bar bars[6];
    char driver_name[32];
    struct pci_device *next;
};

void pci_init(void);
uint32_t pci_read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
void pci_write_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);
void pci_write_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val);

struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id);
struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass);
struct pci_device *pci_get_device_list(void);

#endif /* _AURORA_DRIVERS_PCI_H */
