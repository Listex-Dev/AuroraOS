#ifndef _AURORA_NET_ARP_H
#define _AURORA_NET_ARP_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>
#include <aurora/net/netbuf.h>

#define ARPHRD_ETHER 1
#define ARPOP_REQUEST 1
#define ARPOP_REPLY   2

struct arphdr {
    uint16_t ar_hrd;          /* Format of hardware address */
    uint16_t ar_pro;          /* Format of protocol address */
    uint8_t  ar_hln;          /* Length of hardware address (6) */
    uint8_t  ar_pln;          /* Length of protocol address (4) */
    uint16_t ar_op;           /* ARP opcode (command) */
    uint8_t  ar_sha[ETH_ALEN];/* Sender hardware address */
    uint32_t ar_sip;          /* Sender IP address */
    uint8_t  ar_tha[ETH_ALEN];/* Target hardware address */
    uint32_t ar_tip;          /* Target IP address */
} __attribute__((packed));

struct arp_entry {
    uint32_t ip;              /* Network byte order */
    uint8_t  mac[ETH_ALEN];
    uint32_t flags;
    uint64_t timestamp;
    bool     valid;
};

#define ARP_TABLE_SIZE 64

void arp_init(void);
int arp_receive(struct netdev *dev, netbuf_t *nb);
int arp_send_request(struct netdev *dev, uint32_t target_ip);
int arp_send_reply(struct netdev *dev, const uint8_t *target_mac, uint32_t target_ip);
int arp_lookup(struct netdev *dev, uint32_t ip, uint8_t *mac_out);
void arp_cache_insert(uint32_t ip, const uint8_t *mac);
const struct arp_entry *arp_get_table(void);

#endif /* _AURORA_NET_ARP_H */
