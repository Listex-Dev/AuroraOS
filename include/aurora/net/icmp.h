#ifndef _AURORA_NET_ICMP_H
#define _AURORA_NET_ICMP_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>
#include <aurora/net/netbuf.h>

#define ICMP_ECHOREPLY      0
#define ICMP_DEST_UNREACH   3
#define ICMP_ECHO           8
#define ICMP_TIME_EXCEEDED  11

struct icmphdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    union {
        struct {
            uint16_t id;
            uint16_t sequence;
        } echo;
        uint32_t gateway;
        struct {
            uint16_t unused;
            uint16_t mtu;
        } frag;
    } un;
} __attribute__((packed));

void icmp_init(void);
int icmp_receive(struct netdev *dev, uint32_t src_ip, netbuf_t *nb);
int icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq, const void *data, size_t len);
int icmp_wait_reply(uint16_t id, uint16_t seq, uint32_t timeout_ms);

#endif /* _AURORA_NET_ICMP_H */
