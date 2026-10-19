#include <aurora/drivers/e1000.h>
#include <aurora/drivers/pci.h>
#include <aurora/arch/x86_64/interrupts.h>
#include <aurora/arch/x86_64/pic.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static void *s_e1000_mmio = NULL;
static struct netdev s_e1000_netdev;

static volatile struct e1000_rx_desc *s_rx_desc = NULL;
static uint64_t s_rx_phys_bufs[E1000_NUM_RX_DESC];
static void *s_rx_virt_bufs[E1000_NUM_RX_DESC];
static uint32_t s_rx_cur = 0;

static volatile struct e1000_tx_desc *s_tx_desc = NULL;
static uint64_t s_tx_phys_bufs[E1000_NUM_TX_DESC];
static void *s_tx_virt_bufs[E1000_NUM_TX_DESC];
static uint32_t s_tx_cur = 0;

static inline void e1000_write32(uint32_t reg, uint32_t val)
{
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)s_e1000_mmio + reg);
    *addr = val;
}

static inline uint32_t e1000_read32(uint32_t reg)
{
    volatile uint32_t *addr = (volatile uint32_t *)((uint8_t *)s_e1000_mmio + reg);
    return *addr;
}

static int e1000_transmit(struct netdev *dev, netbuf_t *nb)
{
    if (!dev || !nb || !s_e1000_mmio || nb->len > 1518) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    uint32_t idx = s_tx_cur;
    memcpy(s_tx_virt_bufs[idx], nb->data, nb->len);

    s_tx_desc[idx].addr = s_tx_phys_bufs[idx];
    s_tx_desc[idx].length = (uint16_t)nb->len;
    s_tx_desc[idx].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    s_tx_desc[idx].status = 0;

    s_tx_cur = (s_tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write32(E1000_REG_TDT, s_tx_cur);

    dev->stats.tx_packets++;
    dev->stats.tx_bytes += nb->len;

    netbuf_free(nb);

    /* Poll briefly for completion */
    for (int i = 0; i < 1000; i++) {
        if (s_tx_desc[idx].status & E1000_TXD_STAT_DD) {
            break;
        }
    }

    return 0;
}

int e1000_poll_rx(void)
{
    if (!s_e1000_mmio || !s_rx_desc) {
        return 0;
    }

    /* Force QEMU SLIRP emulation update */
    e1000_read32(E1000_REG_STATUS);

    int received = 0;
    while (s_rx_desc[s_rx_cur].status & E1000_RXD_STAT_DD) {
        uint16_t len = s_rx_desc[s_rx_cur].length;
        if (len > 0) {
            netbuf_t *nb = netbuf_alloc(len);
            if (nb) {
                uint8_t *dst = netbuf_put(nb, len);
                if (dst) {
                    memcpy(dst, s_rx_virt_bufs[s_rx_cur], len);
                    netdev_receive(&s_e1000_netdev, nb);
                    received++;
                } else {
                    netbuf_free(nb);
                }
            }
        }

        s_rx_desc[s_rx_cur].status = 0;
        uint32_t old_tail = s_rx_cur;
        s_rx_cur = (s_rx_cur + 1) % E1000_NUM_RX_DESC;
        e1000_write32(E1000_REG_RDT, old_tail);
    }

    return received;
}

static int e1000_netdev_poll(struct netdev *dev)
{
    (void)dev;
    return e1000_poll_rx();
}

static struct netdev_ops s_e1000_ops = {
    .init     = NULL,
    .transmit = e1000_transmit,
    .poll     = e1000_netdev_poll
};

static void e1000_irq_handler(interrupt_frame_t *frame)
{
    (void)frame;
    if (s_e1000_mmio) {
        /* Reading ICR acknowledges and clears interrupts */
        uint32_t icr = e1000_read32(E1000_REG_ICR);
        (void)icr;
        e1000_poll_rx();
    }
    pic_send_eoi(11);
}

int e1000_init(void)
{
    /* Find Intel e1000 / e1000e NIC in PCI device list */
    struct pci_device *pdev = pci_find_device(0x8086, 0x100E); /* 82540EM */
    if (!pdev) {
        pdev = pci_find_device(0x8086, 0x10D3); /* 82574L */
    }
    if (!pdev) {
        pdev = pci_find_class(0x02, 0x00); /* Network Controller, Ethernet */
    }

    if (!pdev) {
        printk("<4>[E1000] No supported Intel e1000 Ethernet adapter found on PCI bus\n");
        return -1;
    }

    printk("<6>[E1000] Found Ethernet controller at %02x:%02x.%x (Vendor 0x%04x, Device 0x%04x, IRQ %d)\n",
           pdev->bus, pdev->slot, pdev->func, pdev->vendor_id, pdev->device_id, pdev->irq_line);

    /* 1. Enable Bus Master & Memory Space in PCI Command register */
    uint16_t pci_cmd = pci_read_config16(pdev->bus, pdev->slot, pdev->func, 0x04);
    pci_cmd |= (1 << 1) | (1 << 2); /* Memory space + Bus master */
    pci_write_config16(pdev->bus, pdev->slot, pdev->func, 0x04, pci_cmd);

    /* 2. Map BAR0 MMIO space (128 KB) */
    uint64_t bar0_phys = pdev->bars[0].base;
    s_e1000_mmio = vmm_map_mmio(bar0_phys, 0x20000);
    if (!s_e1000_mmio) {
        printk("<3>[E1000] Failed to map BAR0 MMIO space at 0x%llx\n", bar0_phys);
        return -1;
    }

    /* 3. Reset device */
    e1000_write32(E1000_REG_CTRL, e1000_read32(E1000_REG_CTRL) | E1000_CTRL_RST);
    for (volatile int d = 0; d < 50000; d++);

    /* Re-enable Bus Master & Memory Space in PCI Command register after reset */
    pci_cmd = pci_read_config16(pdev->bus, pdev->slot, pdev->func, 0x04);
    pci_cmd |= (1 << 0) | (1 << 1) | (1 << 2);
    pci_write_config16(pdev->bus, pdev->slot, pdev->func, 0x04, pci_cmd);

    /* Set Link Up and auto-speed */
    e1000_write32(E1000_REG_CTRL, E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD);

    uint32_t status = e1000_read32(E1000_REG_STATUS);
    printk("<6>[E1000] Status register: 0x%x (Link %s)\n", status, (status & 2) ? "UP" : "DOWN");

    /* Clear Multicast Table Array */
    for (int i = 0; i < 128; i++) {
        e1000_write32(E1000_REG_MTA + (i * 4), 0);
    }

    /* 4. Read MAC address from RAL0/RAH0 or EEPROM */
    uint32_t ral = e1000_read32(E1000_REG_RAL);
    uint32_t rah = e1000_read32(E1000_REG_RAH);

    uint8_t mac[ETH_ALEN];
    mac[0] = (uint8_t)(ral & 0xFF);
    mac[1] = (uint8_t)((ral >> 8) & 0xFF);
    mac[2] = (uint8_t)((ral >> 16) & 0xFF);
    mac[3] = (uint8_t)((ral >> 24) & 0xFF);
    mac[4] = (uint8_t)(rah & 0xFF);
    mac[5] = (uint8_t)((rah >> 8) & 0xFF);

    /* Fallback to default QEMU MAC if unassigned */
    if ((mac[0] == 0 && mac[1] == 0 && mac[2] == 0) || (mac[0] == 0xFF && mac[1] == 0xFF)) {
        mac[0] = 0x52; mac[1] = 0x54; mac[2] = 0x00;
        mac[3] = 0x12; mac[4] = 0x34; mac[5] = 0x56;
        e1000_write32(E1000_REG_RAL, 0x12005452);
        e1000_write32(E1000_REG_RAH, 0x80005634);
    }

    printk("<6>[E1000] Hardware MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* 5. Initialize RX Ring */
    uintptr_t rx_ring_phys = pmm_alloc_page();
    s_rx_desc = (volatile struct e1000_rx_desc *)vmm_phys_to_virt(rx_ring_phys);
    memset((void *)s_rx_desc, 0, PAGE_SIZE);

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        uintptr_t buf_phys = pmm_alloc_page();
        s_rx_phys_bufs[i] = buf_phys;
        s_rx_virt_bufs[i] = vmm_phys_to_virt(buf_phys);
        s_rx_desc[i].addr = buf_phys;
        s_rx_desc[i].status = 0;
    }

    e1000_write32(E1000_REG_RDBAL, (uint32_t)rx_ring_phys);
    e1000_write32(E1000_REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    e1000_write32(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    e1000_write32(E1000_REG_RDH, 0);
    e1000_write32(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);
    s_rx_cur = 0;

    e1000_write32(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_BAM | E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_BSIZE_2048 | E1000_RCTL_SECRC);

    /* 6. Initialize TX Ring */
    uintptr_t tx_ring_phys = pmm_alloc_page();
    s_tx_desc = (volatile struct e1000_tx_desc *)vmm_phys_to_virt(tx_ring_phys);
    memset((void *)s_tx_desc, 0, PAGE_SIZE);

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        uintptr_t buf_phys = pmm_alloc_page();
        s_tx_phys_bufs[i] = buf_phys;
        s_tx_virt_bufs[i] = vmm_phys_to_virt(buf_phys);
        s_tx_desc[i].addr = buf_phys;
        s_tx_desc[i].status = 0;
    }

    e1000_write32(E1000_REG_TDBAL, (uint32_t)tx_ring_phys);
    e1000_write32(E1000_REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    e1000_write32(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    e1000_write32(E1000_REG_TDH, 0);
    e1000_write32(E1000_REG_TDT, 0);
    s_tx_cur = 0;

    e1000_write32(E1000_REG_TIPG, 0x0060200A);
    e1000_write32(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << E1000_TCTL_CT_SHIFT) | (0x40 << E1000_TCTL_COLD_SHIFT));

    uint16_t cmd_chk = pci_read_config16(pdev->bus, pdev->slot, pdev->func, 0x04);
    uint32_t rctl_chk = e1000_read32(E1000_REG_RCTL);
    uint32_t tctl_chk = e1000_read32(E1000_REG_TCTL);
    printk("<6>[E1000] Status: CMD=0x%04x, RCTL=0x%08x, TCTL=0x%08x\n", cmd_chk, rctl_chk, tctl_chk);

    /* 7. Setup Interrupts */
    e1000_read32(E1000_REG_ICR); /* Clear pending */
    e1000_write32(E1000_REG_IMS, E1000_ICR_RXT0 | E1000_ICR_RXDMT0 | E1000_ICR_TXDW | E1000_ICR_LSC);

    uint8_t irq = pdev->irq_line ? pdev->irq_line : 11;
    isr_register_handler(PIC_MASTER_OFFSET + irq, e1000_irq_handler);
    pic_clear_mask(irq);

    /* 8. Register eth0 interface */
    memset(&s_e1000_netdev, 0, sizeof(s_e1000_netdev));
    strncpy(s_e1000_netdev.name, "eth0", sizeof(s_e1000_netdev.name) - 1);
    memcpy(s_e1000_netdev.hwaddr, mac, ETH_ALEN);

    /* Standard QEMU user networking IP configuration: 10.0.2.15 */
    s_e1000_netdev.ip_addr = net_htonl(0x0A00020F);
    s_e1000_netdev.netmask = net_htonl(0xFFFFFF00);
    s_e1000_netdev.gateway = net_htonl(0x0A000202);

    s_e1000_netdev.flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING;
    s_e1000_netdev.mtu = 1500;
    s_e1000_netdev.ops = &s_e1000_ops;

    netdev_register(&s_e1000_netdev);

    printk("<6>[  0.000000] Intel e1000 Gigabit Ethernet driver active on eth0 (IP 10.0.2.15, GW 10.0.2.2)\n");
    return 0;
}
