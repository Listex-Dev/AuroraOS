#ifndef _AURORA_DRIVERS_AHCI_H
#define _AURORA_DRIVERS_AHCI_H

#include <aurora/types.h>
#include <aurora/drivers/pci.h>
#include <aurora/drivers/block.h>

#define HBA_PORT_IPM_ACTIVE  1
#define HBA_PORT_DET_PRESENT 3

#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101
#define SATA_SIG_SEMB   0xC33C0101
#define SATA_SIG_PM     0x96690101

#define AHCI_CMD_READ_DMA_EXT   0x25
#define AHCI_CMD_WRITE_DMA_EXT  0x35
#define AHCI_CMD_FLUSH_CACHE_EXT 0xEA
#define AHCI_CMD_IDENTIFY       0xEC

typedef volatile struct hba_port {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} hba_port_t;

typedef volatile struct hba_mem {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} hba_mem_t;

typedef struct hba_cmd_header {
    uint8_t  cfl:5;
    uint8_t  a:1;
    uint8_t  w:1;
    uint8_t  p:1;
    uint8_t  r:1;
    uint8_t  b:1;
    uint8_t  c:1;
    uint8_t  reserved0:1;
    uint8_t  pmp:4;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct hba_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc:22;
    uint32_t reserved1:9;
    uint32_t i:1;
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct hba_cmd_tbl {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  reserved[48];
    hba_prdt_entry_t prdt_entry[1];
} __attribute__((packed)) hba_cmd_tbl_t;

typedef struct fis_reg_h2d {
    uint8_t  fis_type;    /* 0x27 */
    uint8_t  pmport:4;
    uint8_t  reserved0:3;
    uint8_t  c:1;         /* 1: Command, 0: Control */
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  featureh;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  reserved1[4];
} __attribute__((packed)) fis_reg_h2d_t;

int ahci_init(void);

#endif /* _AURORA_DRIVERS_AHCI_H */
