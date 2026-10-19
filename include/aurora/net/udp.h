#ifndef _AURORA_NET_UDP_H
#define _AURORA_NET_UDP_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>
#include <aurora/net/netbuf.h>

struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
} __attribute__((packed));

struct socket;

void udp_init(void);
int udp_receive(struct netdev *dev, uint32_t src_ip, uint32_t dst_ip, netbuf_t *nb);
int udp_send(struct socket *sock, uint32_t dst_ip, uint16_t dst_port, const void *buf, size_t len);
int udp_bind(struct socket *sock, uint16_t port);
void udp_unbind(struct socket *sock);
struct socket *udp_get_socket_list(void);

#endif /* _AURORA_NET_UDP_H */
