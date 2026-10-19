#include <aurora/net/ethernet.h>
#include <aurora/net/arp.h>
#include <aurora/net/ipv4.h>
#include <aurora/string.h>

int ethernet_receive(struct netdev *dev, netbuf_t *nb)
{
    if (!dev || !nb || nb->len < sizeof(struct ethhdr)) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct ethhdr *eth = (struct ethhdr *)nb->data;
    uint16_t proto = net_ntohs(eth->h_proto);

    /* Strip Ethernet header */
    netbuf_pull(nb, sizeof(struct ethhdr));

    if (proto == ETH_P_ARP) {
        return arp_receive(dev, nb);
    } else if (proto == ETH_P_IP) {
        return ipv4_receive(dev, nb);
    }

    netbuf_free(nb);
    return 0;
}

int ethernet_send(struct netdev *dev, const uint8_t *dst_mac, uint16_t ethertype, netbuf_t *nb)
{
    if (!dev || !nb) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct ethhdr *eth = (struct ethhdr *)netbuf_push(nb, sizeof(struct ethhdr));
    if (!eth) {
        netbuf_free(nb);
        return -1;
    }

    if (dst_mac) {
        memcpy(eth->h_dest, dst_mac, ETH_ALEN);
    } else {
        /* Default to broadcast */
        memset(eth->h_dest, 0xFF, ETH_ALEN);
    }

    memcpy(eth->h_source, dev->hwaddr, ETH_ALEN);
    eth->h_proto = net_htons(ethertype);

    /* Pad frame to minimum 60 bytes */
    if (nb->len < 60) {
        size_t pad = 60 - nb->len;
        uint8_t *p = netbuf_put(nb, pad);
        if (p) {
            memset(p, 0, pad);
        }
    }

    if (dev->ops && dev->ops->transmit) {
        return dev->ops->transmit(dev, nb);
    }

    netbuf_free(nb);
    return -1;
}
