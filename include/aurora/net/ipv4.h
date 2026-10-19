#ifndef _AURORA_NET_IPV4_H
#define _AURORA_NET_IPV4_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>
#include <aurora/net/netbuf.h>

#define IPPROTO_ICMP 1
#define IPPROTO_TCP  6
#define IPPROTO_UDP  17

struct iphdr {
    uint8_t  ihl:4;
    uint8_t  version:4;
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
} __attribute__((packed));

uint16_t ip_checksum(const void *buf, size_t len);
int ipv4_receive(struct netdev *dev, netbuf_t *nb);
int ipv4_send(struct netdev *dev, uint32_t dst_ip, uint8_t protocol, netbuf_t *nb);
struct netdev *ipv4_route(uint32_t dst_ip, uint32_t *next_hop_ip);

#endif /* _AURORA_NET_IPV4_H */
