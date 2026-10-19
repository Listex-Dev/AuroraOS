#ifndef _AURORA_NET_NETDEV_H
#define _AURORA_NET_NETDEV_H

#include <aurora/types.h>
#include <aurora/net/netbuf.h>

#define IFNAMSIZ 16

#define IFF_UP        0x1
#define IFF_BROADCAST 0x2
#define IFF_LOOPBACK  0x8
#define IFF_RUNNING   0x40

#define ETH_ALEN 6

struct netdev;

struct netdev_ops {
    int (*init)(struct netdev *dev);
    int (*transmit)(struct netdev *dev, netbuf_t *nb);
    int (*poll)(struct netdev *dev);
};

struct netdev_stats {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
};

struct netdev {
    char name[IFNAMSIZ];
    uint8_t hwaddr[ETH_ALEN];
    uint32_t ip_addr;     /* IPv4 address in network byte order */
    uint32_t netmask;     /* Subnet mask in network byte order */
    uint32_t gateway;     /* Default gateway in network byte order */
    uint32_t flags;
    uint32_t mtu;
    struct netdev_ops *ops;
    struct netdev_stats stats;
    void *priv;
    struct netdev *next;
};

/* Endian helpers */
static inline uint16_t net_htons(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}
static inline uint16_t net_ntohs(uint16_t v) {
    return net_htons(v);
}
static inline uint32_t net_htonl(uint32_t v) {
    return ((v >> 24) & 0xff) |
           ((v >> 8)  & 0xff00) |
           ((v << 8)  & 0xff0000) |
           ((v << 24) & 0xff000000);
}
static inline uint32_t net_ntohl(uint32_t v) {
    return net_htonl(v);
}

void netdev_subsys_init(void);
int netdev_register(struct netdev *dev);
struct netdev *netdev_get_by_name(const char *name);
struct netdev *netdev_get_default(void);
struct netdev *netdev_get_list(void);
int netdev_receive(struct netdev *dev, netbuf_t *nb);
void loopback_init(void);

#endif /* _AURORA_NET_NETDEV_H */
