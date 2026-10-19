#ifndef _AURORA_DRIVERS_E1000_H
#define _AURORA_DRIVERS_E1000_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>

#define E1000_REG_CTRL      0x00000 /* Device Control */
#define E1000_REG_STATUS    0x00008 /* Device Status */
#define E1000_REG_EECD      0x00010 /* EEPROM Control */
#define E1000_REG_EERD      0x00014 /* EEPROM Read */
#define E1000_REG_CTRL_EXT  0x00018 /* Extended Device Control */
#define E1000_REG_ICR       0x000C0 /* Interrupt Cause Read */
#define E1000_REG_ITR       0x000C4 /* Interrupt Throttling Rate */
#define E1000_REG_ICS       0x000C8 /* Interrupt Cause Set */
#define E1000_REG_IMS       0x000D0 /* Interrupt Mask Set */
#define E1000_REG_IMC       0x000D8 /* Interrupt Mask Clear */
#define E1000_REG_RCTL      0x00100 /* Receive Control */
#define E1000_REG_TCTL      0x00400 /* Transmit Control */
#define E1000_REG_TIPG      0x00410 /* Transmit IPG */
#define E1000_REG_RDBAL     0x02800 /* RX Descriptor Base Low */
#define E1000_REG_RDBAH     0x02804 /* RX Descriptor Base High */
#define E1000_REG_RDLEN     0x02808 /* RX Descriptor Length */
#define E1000_REG_RDH       0x02810 /* RX Descriptor Head */
#define E1000_REG_RDT       0x02818 /* RX Descriptor Tail */
#define E1000_REG_TDBAL     0x03800 /* TX Descriptor Base Low */
#define E1000_REG_TDBAH     0x03804 /* TX Descriptor Base High */
#define E1000_REG_TDLEN     0x03808 /* TX Descriptor Length */
#define E1000_REG_TDH       0x03810 /* TX Descriptor Head */
#define E1000_REG_TDT       0x03818 /* TX Descriptor Tail */
#define E1000_REG_MTA       0x05200 /* Multicast Table Array (128 * 4) */
#define E1000_REG_RAL       0x05400 /* Receive Address Low */
#define E1000_REG_RAH       0x05404 /* Receive Address High */

/* CTRL register bits */
#define E1000_CTRL_FD       (1 << 0)  /* Full Duplex */
#define E1000_CTRL_ASDE     (1 << 5)  /* Auto-Speed Detection Enable */
#define E1000_CTRL_SLU      (1 << 6)  /* Set Link Up */
#define E1000_CTRL_RST      (1 << 26) /* Full Device Reset */

/* RCTL register bits */
#define E1000_RCTL_EN       (1 << 1)  /* Receiver Enable */
#define E1000_RCTL_SBP      (1 << 2)  /* Store Bad Packets */
#define E1000_RCTL_UPE      (1 << 3)  /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE      (1 << 4)  /* Multicast Promiscuous Enable */
#define E1000_RCTL_LPE      (1 << 5)  /* Long Packet Enable */
#define E1000_RCTL_BAM      (1 << 15) /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2048 (0 << 16)
#define E1000_RCTL_SECRC    (1 << 26) /* Strip Ethernet CRC */

/* TCTL register bits */
#define E1000_TCTL_EN       (1 << 1)  /* Transmit Enable */
#define E1000_TCTL_PSP      (1 << 3)  /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT 4         /* Collision Threshold (0x10) */
#define E1000_TCTL_COLD_SHIFT 12      /* Collision Distance (0x40) */

/* TX Descriptor Command bits */
#define E1000_TXD_CMD_EOP   (1 << 0)  /* End of Packet */
#define E1000_TXD_CMD_IFCS  (1 << 1)  /* Insert FCS */
#define E1000_TXD_CMD_RS    (1 << 3)  /* Report Status */

/* Descriptor Status bits */
#define E1000_TXD_STAT_DD   (1 << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_DD   (1 << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_EOP  (1 << 1)  /* End of Packet */

/* ICR/IMS bits */
#define E1000_ICR_TXDW      (1 << 0)  /* Transmit Descriptor Written Back */
#define E1000_ICR_TXQE      (1 << 1)  /* Transmit Queue Empty */
#define E1000_ICR_LSC       (1 << 2)  /* Link Status Change */
#define E1000_ICR_RXSEQ     (1 << 3)  /* Receive Sequence Error */
#define E1000_ICR_RXDMT0    (1 << 4)  /* Receive Descriptor Min Threshold */
#define E1000_ICR_RXT0      (1 << 7)  /* Receiver Timer Interrupt */

#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   32

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

int e1000_init(void);
int e1000_poll_rx(void);

#endif /* _AURORA_DRIVERS_E1000_H */
