#ifndef _AURORA_NET_TCP_H
#define _AURORA_NET_TCP_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>
#include <aurora/net/netbuf.h>

struct tcphdr {
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
    uint8_t  res1:4;
    uint8_t  doff:4;
    uint8_t  flags;
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
} __attribute__((packed));

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

enum tcp_state {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
};

struct socket;

void tcp_init(void);
int tcp_receive(struct netdev *dev, uint32_t src_ip, uint32_t dst_ip, netbuf_t *nb);
int tcp_connect(struct socket *sock, uint32_t dst_ip, uint16_t dst_port);
int tcp_bind(struct socket *sock, uint16_t port);
int tcp_listen(struct socket *sock, int backlog);
struct socket *tcp_accept(struct socket *sock);
int tcp_send(struct socket *sock, const void *buf, size_t len);
int tcp_close(struct socket *sock);
void tcp_unbind(struct socket *sock);

#endif /* _AURORA_NET_TCP_H */
