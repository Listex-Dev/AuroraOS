#include <aurora/net/ipv4.h>
#include <aurora/net/ethernet.h>
#include <aurora/net/arp.h>
#include <aurora/net/icmp.h>
#include <aurora/net/udp.h>
#include <aurora/net/tcp.h>
#include <aurora/string.h>

static uint16_t s_ip_ident = 1;

uint16_t ip_checksum(const void *buf, size_t len)
{
    const uint16_t *ptr = (const uint16_t *)buf;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const uint8_t *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

struct netdev *ipv4_route(uint32_t dst_ip, uint32_t *next_hop_ip)
{
    /* Loopback check (127.0.0.0/8) */
    if ((dst_ip & net_htonl(0xFF000000)) == net_htonl(0x7F000000)) {
        if (next_hop_ip) {
            *next_hop_ip = dst_ip;
        }
        return netdev_get_by_name("lo");
    }

    /* Check physical interfaces */
    struct netdev *dev = netdev_get_default();
    if (dev && (dev->flags & IFF_UP)) {
        if ((dst_ip & dev->netmask) == (dev->ip_addr & dev->netmask)) {
            if (next_hop_ip) {
                *next_hop_ip = dst_ip;
            }
        } else {
            if (next_hop_ip) {
                *next_hop_ip = dev->gateway ? dev->gateway : dst_ip;
            }
        }
        return dev;
    }

    /* Fallback to loopback */
    if (next_hop_ip) {
        *next_hop_ip = dst_ip;
    }
    return netdev_get_by_name("lo");
}

int ipv4_send(struct netdev *dev, uint32_t dst_ip, uint8_t protocol, netbuf_t *nb)
{
    if (!nb) {
        return -1;
    }

    uint32_t next_hop = dst_ip;
    if (!dev) {
        dev = ipv4_route(dst_ip, &next_hop);
    }
    if (!dev) {
        netbuf_free(nb);
        return -1;
    }

    struct iphdr *ip = (struct iphdr *)netbuf_push(nb, sizeof(struct iphdr));
    if (!ip) {
        netbuf_free(nb);
        return -1;
    }

    ip->version  = 4;
    ip->ihl      = 5;
    ip->tos      = 0;
    ip->tot_len  = net_htons((uint16_t)nb->len);
    ip->id       = net_htons(s_ip_ident++);
    ip->frag_off = net_htons(0x4000); /* Don't Fragment (DF) */
    ip->ttl      = 64;
    ip->protocol = protocol;
    ip->check    = 0;
    ip->saddr    = dev->ip_addr;
    ip->daddr    = dst_ip;
    ip->check    = ip_checksum(ip, sizeof(struct iphdr));

    /* Loopback sends directly without Ethernet encapsulation */
    if (dev->flags & IFF_LOOPBACK) {
        if (dev->ops && dev->ops->transmit) {
            return dev->ops->transmit(dev, nb);
        }
        netbuf_free(nb);
        return -1;
    }

    /* Resolve next hop MAC via ARP */
    uint8_t dst_mac[ETH_ALEN];
    if (arp_lookup(dev, next_hop, dst_mac) < 0) {
        netbuf_free(nb);
        return -1;
    }

    return ethernet_send(dev, dst_mac, ETH_P_IP, nb);
}

int ipv4_receive(struct netdev *dev, netbuf_t *nb)
{
    if (!dev || !nb || nb->len < sizeof(struct iphdr)) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct iphdr *ip = (struct iphdr *)nb->data;
    if (ip->version != 4) {
        netbuf_free(nb);
        return -1;
    }

    uint32_t src_ip = ip->saddr;
    uint32_t dst_ip = ip->daddr;
    uint8_t proto = ip->protocol;
    size_t hdr_len = ip->ihl * 4;

    if (nb->len < hdr_len) {
        netbuf_free(nb);
        return -1;
    }

    netbuf_pull(nb, hdr_len);

    if (proto == IPPROTO_ICMP) {
        return icmp_receive(dev, src_ip, nb);
    } else if (proto == IPPROTO_UDP) {
        return udp_receive(dev, src_ip, dst_ip, nb);
    } else if (proto == IPPROTO_TCP) {
        return tcp_receive(dev, src_ip, dst_ip, nb);
    }

    netbuf_free(nb);
    return 0;
}
