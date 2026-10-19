#ifndef _AURORA_NET_ETHERNET_H
#define _AURORA_NET_ETHERNET_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>
#include <aurora/net/netbuf.h>

#define ETH_P_IP   0x0800
#define ETH_P_ARP  0x0806

struct ethhdr {
    uint8_t  h_dest[ETH_ALEN];
    uint8_t  h_source[ETH_ALEN];
    uint16_t h_proto;
} __attribute__((packed));

int ethernet_receive(struct netdev *dev, netbuf_t *nb);
int ethernet_send(struct netdev *dev, const uint8_t *dst_mac, uint16_t ethertype, netbuf_t *nb);

#endif /* _AURORA_NET_ETHERNET_H */
