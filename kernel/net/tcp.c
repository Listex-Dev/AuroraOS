#include <aurora/net/tcp.h>
#include <aurora/net/ipv4.h>
#include <aurora/net/socket.h>
#include <aurora/mm/heap.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static struct socket *s_tcp_sockets = NULL;
static uint16_t s_next_tcp_port = 49152;
static uint32_t s_isn_counter = 10000;

struct tcp_pseudo_hdr {
    uint32_t src;
    uint32_t dst;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t length;
} __attribute__((packed));

void tcp_init(void)
{
    s_tcp_sockets = NULL;
    s_next_tcp_port = 49152;
    s_isn_counter = 10000;
    printk("<6>[  0.000000] Net: TCP subsystem initialized\n");
}

int tcp_bind(struct socket *sock, uint16_t port)
{
    if (!sock) {
        return -1;
    }

    if (port == 0) {
        port = s_next_tcp_port++;
        if (s_next_tcp_port > 65000) {
            s_next_tcp_port = 49152;
        }
    }

    sock->local_port = port;

    /* Add to list if not present */
    for (struct socket *c = s_tcp_sockets; c != NULL; c = c->next) {
        if (c == sock) {
            return 0;
        }
    }

    sock->next = s_tcp_sockets;
    s_tcp_sockets = sock;
    return 0;
}

void tcp_unbind(struct socket *sock)
{
    if (!sock) {
        return;
    }

    struct socket **curr = &s_tcp_sockets;
    while (*curr) {
        if (*curr == sock) {
            *curr = sock->next;
            sock->next = NULL;
            return;
        }
        curr = &(*curr)->next;
    }
}

static uint16_t tcp_checksum(uint32_t saddr, uint32_t daddr, struct tcphdr *tcp, size_t tcp_len)
{
    uint32_t sum = 0;

    /* Pseudo-header */
    sum += (uint16_t)(saddr & 0xFFFF);
    sum += (uint16_t)(saddr >> 16);
    sum += (uint16_t)(daddr & 0xFFFF);
    sum += (uint16_t)(daddr >> 16);
    sum += (uint16_t)net_htons(IPPROTO_TCP);
    sum += (uint16_t)net_htons((uint16_t)tcp_len);

    /* TCP header + payload */
    const uint8_t *ptr = (const uint8_t *)tcp;
    size_t len = tcp_len;
    while (len > 1) {
        sum += ((uint16_t)ptr[1] << 8) | (uint16_t)ptr[0];
        ptr += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += (uint16_t)(*ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

static int tcp_send_packet(struct socket *sock, uint8_t flags, const void *data, size_t data_len)
{
    if (!sock) {
        return -1;
    }

    size_t total_len = sizeof(struct tcphdr) + data_len;
    netbuf_t *nb = netbuf_alloc(total_len);
    if (!nb) {
        return -1;
    }

    struct tcphdr *tcp = (struct tcphdr *)netbuf_put(nb, total_len);
    if (!tcp) {
        netbuf_free(nb);
        return -1;
    }

    tcp->source  = net_htons(sock->local_port);
    tcp->dest    = net_htons(sock->remote_port);
    tcp->seq     = net_htonl(sock->snd_nxt);
    tcp->ack_seq = net_htonl(sock->rcv_nxt);
    tcp->doff    = sizeof(struct tcphdr) / 4;
    tcp->res1    = 0;
    tcp->flags   = flags;
    uint32_t win_avail = (SOCKET_BUFFER_SIZE > sock->rx_count) ? (SOCKET_BUFFER_SIZE - sock->rx_count) : 0;
    if (win_avail > 65535) win_avail = 65535;
    tcp->window = net_htons((uint16_t)win_avail);
    tcp->check = 0;
    tcp->urg_ptr = 0;

    if (data && data_len > 0) {
        memcpy((uint8_t *)tcp + sizeof(struct tcphdr), data, data_len);
    }

    /* Route to find source IP */
    uint32_t next_hop = 0;
    struct netdev *dev = ipv4_route(sock->remote_ip, &next_hop);
    uint32_t saddr = dev ? dev->ip_addr : net_htonl(0x7F000001);

    tcp->check = tcp_checksum(saddr, sock->remote_ip, tcp, total_len);

    return ipv4_send(dev, sock->remote_ip, IPPROTO_TCP, nb);
}

int tcp_connect(struct socket *sock, uint32_t dst_ip, uint16_t dst_port)
{
    if (!sock) {
        return -1;
    }

    if (sock->local_port == 0) {
        tcp_bind(sock, 0);
    }

    sock->remote_ip = dst_ip;
    sock->remote_port = dst_port;
    sock->snd_nxt = s_isn_counter += 1000;
    sock->rcv_nxt = 0;
    sock->state = TCP_SYN_SENT;

    /* Send SYN */
    tcp_send_packet(sock, TCP_FLAG_SYN, NULL, 0);
    sock->snd_nxt++;

    /* Poll / wait for SYN-ACK */
    struct netdev *dev = netdev_get_default();
    for (int iter = 0; iter < 1000; iter++) {
        if (sock->state == TCP_ESTABLISHED) {
            return 0;
        }

        if (dev && dev->ops && dev->ops->poll) {
            dev->ops->poll(dev);
        }

        for (volatile int d = 0; d < 1000; d++);
    }

    return -1; /* Timeout */
}

int tcp_listen(struct socket *sock, int backlog)
{
    if (!sock) {
        return -1;
    }

    sock->state = TCP_LISTEN;
    sock->backlog = (backlog > 0 && backlog <= 16) ? backlog : 16;
    sock->accept_head = 0;
    sock->accept_tail = 0;
    sock->accept_count = 0;

    if (sock->local_port == 0) {
        tcp_bind(sock, 0);
    }

    return 0;
}

struct socket *tcp_accept(struct socket *sock)
{
    if (!sock || sock->state != TCP_LISTEN) {
        return NULL;
    }

    struct netdev *dev = netdev_get_default();
    for (int iter = 0; iter < 1000; iter++) {
        if (sock->accept_count > 0) {
            struct socket *child = sock->accept_queue[sock->accept_tail];
            if (child && child->state == TCP_ESTABLISHED) {
                sock->accept_queue[sock->accept_tail] = NULL;
                sock->accept_tail = (sock->accept_tail + 1) % 16;
                sock->accept_count--;
                return child;
            }
        }

        if (dev && dev->ops && dev->ops->poll) {
            dev->ops->poll(dev);
        }

        for (volatile int d = 0; d < 1000; d++);
    }

    return NULL;
}

int tcp_send(struct socket *sock, const void *buf, size_t len)
{
    if (!sock) {
        return -1;
    }

    struct netdev *dev = netdev_get_default();
    for (int iter = 0; iter < 1000 && sock->state == TCP_SYN_RECEIVED; iter++) {
        if (dev && dev->ops && dev->ops->poll) {
            dev->ops->poll(dev);
        }
        for (volatile int d = 0; d < 1000; d++);
    }

    if (sock->state != TCP_ESTABLISHED) {
        return -1;
    }

    int ret = tcp_send_packet(sock, TCP_FLAG_ACK | TCP_FLAG_PSH, buf, len);
    if (ret == 0) {
        sock->snd_nxt += (uint32_t)len;
        return (int)len;
    }
    return ret;
}

int tcp_close(struct socket *sock)
{
    if (!sock) {
        return -1;
    }

    if (sock->state == TCP_ESTABLISHED) {
        tcp_send_packet(sock, TCP_FLAG_ACK | TCP_FLAG_FIN, NULL, 0);
        sock->snd_nxt++;
        sock->state = TCP_FIN_WAIT_1;
    }

    tcp_unbind(sock);
    return 0;
}

int tcp_receive(struct netdev *dev, uint32_t src_ip, uint32_t dst_ip, netbuf_t *nb)
{
    (void)dev; (void)dst_ip;
    if (!nb || nb->len < sizeof(struct tcphdr)) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct tcphdr *tcp = (struct tcphdr *)nb->data;
    uint16_t src_port = net_ntohs(tcp->source);
    uint16_t dst_port = net_ntohs(tcp->dest);
    uint32_t seq = net_ntohl(tcp->seq);
    uint32_t ack_seq = net_ntohl(tcp->ack_seq);
    uint8_t flags = tcp->flags;
    size_t hdr_len = tcp->doff * 4;

    /* Match socket */
    struct socket *matched = NULL;
    struct socket *listener = NULL;

    for (struct socket *s = s_tcp_sockets; s != NULL; s = s->next) {
        if (s->local_port == dst_port) {
            if (s->remote_port == src_port && s->remote_ip == src_ip) {
                matched = s;
                break;
            } else if (s->state == TCP_LISTEN) {
                listener = s;
            }
        }
    }

    if (!matched && listener) {
        /* Incoming connection request to listener */
        if (flags & TCP_FLAG_SYN) {
            if (listener->accept_count >= 16) {
                netbuf_free(nb);
                return 0;
            }
            /* Create new child socket */
            struct socket *child = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (child) {
                child->local_port  = listener->local_port;
                child->remote_ip   = src_ip;
                child->remote_port = src_port;
                child->rcv_nxt     = seq + 1;
                child->snd_nxt     = s_isn_counter += 1000;
                child->state       = TCP_SYN_RECEIVED;

                tcp_bind(child, child->local_port);

                /* Send SYN-ACK */
                tcp_send_packet(child, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                child->snd_nxt++;

                /* Enqueue in parent accept queue */
                listener->accept_queue[listener->accept_head] = child;
                listener->accept_head = (listener->accept_head + 1) % 16;
                listener->accept_count++;
            }
        }
        netbuf_free(nb);
        return 0;
    }

    if (matched) {
        if (matched->state == TCP_SYN_SENT) {
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                matched->rcv_nxt = seq + 1;
                matched->state = TCP_ESTABLISHED;

                /* Send ACK */
                tcp_send_packet(matched, TCP_FLAG_ACK, NULL, 0);
            }
        } else if (matched->state == TCP_SYN_RECEIVED) {
            if (flags & TCP_FLAG_ACK) {
                matched->state = TCP_ESTABLISHED;
                size_t payload_len = nb->len > hdr_len ? nb->len - hdr_len : 0;
                if (payload_len > 0) {
                    uint8_t *payload = nb->data + hdr_len;
                    socket_enqueue_rx(matched, payload, payload_len);
                    matched->rcv_nxt = seq + (uint32_t)payload_len;
                    tcp_send_packet(matched, TCP_FLAG_ACK, NULL, 0);
                }
            }
        } else if (matched->state == TCP_ESTABLISHED) {
            size_t payload_len = nb->len > hdr_len ? nb->len - hdr_len : 0;
            if (payload_len > 0) {
                uint8_t *payload = nb->data + hdr_len;
                socket_enqueue_rx(matched, payload, payload_len);
                matched->rcv_nxt = seq + (uint32_t)payload_len;

                /* Send ACK back */
                tcp_send_packet(matched, TCP_FLAG_ACK, NULL, 0);
            }

            if (flags & TCP_FLAG_FIN) {
                matched->rcv_nxt = seq + (uint32_t)payload_len + 1;
                /* Send ACK for FIN */
                tcp_send_packet(matched, TCP_FLAG_ACK, NULL, 0);
                matched->state = TCP_CLOSE_WAIT;
            }
        } else if (matched->state == TCP_FIN_WAIT_1) {
            if (flags & TCP_FLAG_ACK) {
                matched->state = TCP_FIN_WAIT_2;
            }
            if (flags & TCP_FLAG_FIN) {
                matched->rcv_nxt = seq + 1;
                tcp_send_packet(matched, TCP_FLAG_ACK, NULL, 0);
                matched->state = TCP_CLOSED;
            }
        }
    }

    (void)ack_seq;
    netbuf_free(nb);
    return 0;
}

struct socket *tcp_get_socket_list(void)
{
    return s_tcp_sockets;
}

