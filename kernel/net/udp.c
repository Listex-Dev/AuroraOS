#include <aurora/net/udp.h>
#include <aurora/net/ipv4.h>
#include <aurora/net/socket.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static struct socket *s_udp_sockets = NULL;
static uint16_t s_next_ephemeral = 49152;

void udp_init(void)
{
    s_udp_sockets = NULL;
    s_next_ephemeral = 49152;
    printk("<6>[  0.000000] Net: UDP subsystem initialized\n");
}

int udp_bind(struct socket *sock, uint16_t port)
{
    if (!sock) {
        return -1;
    }

    if (port == 0) {
        port = s_next_ephemeral++;
        if (s_next_ephemeral > 65000) {
            s_next_ephemeral = 49152;
        }
    }

    sock->local_port = port;

    /* Add to list if not already present */
    for (struct socket *c = s_udp_sockets; c != NULL; c = c->next) {
        if (c == sock) {
            return 0;
        }
    }

    sock->next = s_udp_sockets;
    s_udp_sockets = sock;
    return 0;
}

void udp_unbind(struct socket *sock)
{
    if (!sock) {
        return;
    }

    struct socket **curr = &s_udp_sockets;
    while (*curr) {
        if (*curr == sock) {
            *curr = sock->next;
            sock->next = NULL;
            return;
        }
        curr = &(*curr)->next;
    }
}

int udp_send(struct socket *sock, uint32_t dst_ip, uint16_t dst_port, const void *buf, size_t len)
{
    if (!sock) {
        return -1;
    }

    if (sock->local_port == 0) {
        udp_bind(sock, 0);
    }

    size_t total_len = sizeof(struct udphdr) + len;
    netbuf_t *nb = netbuf_alloc(total_len);
    if (!nb) {
        return -1;
    }

    struct udphdr *udp = (struct udphdr *)netbuf_put(nb, total_len);
    if (!udp) {
        netbuf_free(nb);
        return -1;
    }

    udp->source = net_htons(sock->local_port);
    udp->dest   = net_htons(dst_port);
    udp->len    = net_htons((uint16_t)total_len);
    udp->check  = 0;

    if (buf && len > 0) {
        memcpy((uint8_t *)udp + sizeof(struct udphdr), buf, len);
    }

    return ipv4_send(NULL, dst_ip, IPPROTO_UDP, nb);
}

int udp_receive(struct netdev *dev, uint32_t src_ip, uint32_t dst_ip, netbuf_t *nb)
{
    (void)dev; (void)dst_ip;
    if (!nb || nb->len < sizeof(struct udphdr)) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct udphdr *udp = (struct udphdr *)nb->data;
    uint16_t dst_port = net_ntohs(udp->dest);
    uint16_t src_port = net_ntohs(udp->source);

    /* Look for matching socket */
    struct socket *matched = NULL;
    for (struct socket *s = s_udp_sockets; s != NULL; s = s->next) {
        if (s->local_port == dst_port) {
            matched = s;
            break;
        }
    }

    if (matched) {
        matched->remote_ip = src_ip;
        matched->remote_port = src_port;

        netbuf_pull(nb, sizeof(struct udphdr));
        socket_enqueue_rx(matched, nb->data, nb->len);
    }

    netbuf_free(nb);
    return 0;
}
