#include <aurora/net/arp.h>
#include <aurora/net/ethernet.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static struct arp_entry s_arp_table[ARP_TABLE_SIZE];
static const uint8_t s_bcast_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void arp_init(void)
{
    memset(s_arp_table, 0, sizeof(s_arp_table));

    /* Pre-seed QEMU SLIRP default gateway (10.0.2.2 -> 52:55:0a:00:02:02) */
    uint32_t gw_ip = net_htonl(0x0A000202);
    uint8_t gw_mac[ETH_ALEN] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x02};
    arp_cache_insert(gw_ip, gw_mac);

    /* Pre-seed QEMU SLIRP DNS server (10.0.2.3 -> 52:55:0a:00:02:03) */
    uint32_t dns_ip = net_htonl(0x0A000203);
    uint8_t dns_mac[ETH_ALEN] = {0x52, 0x55, 0x0A, 0x00, 0x02, 0x03};
    arp_cache_insert(dns_ip, dns_mac);

    printk("<6>[  0.000000] Net: ARP cache initialized\n");
}

void arp_cache_insert(uint32_t ip, const uint8_t *mac)
{
    if (!mac) {
        return;
    }

    /* Check if already present */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            return;
        }
    }

    /* Find empty slot */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!s_arp_table[i].valid) {
            s_arp_table[i].valid = true;
            s_arp_table[i].ip = ip;
            s_arp_table[i].flags = 2; /* ATF_COM (completed) */
            memcpy(s_arp_table[i].mac, mac, ETH_ALEN);
            return;
        }
    }

    /* Overwrite first slot if table full */
    s_arp_table[0].valid = true;
    s_arp_table[0].ip = ip;
    s_arp_table[0].flags = 2;
    memcpy(s_arp_table[0].mac, mac, ETH_ALEN);
}

int arp_lookup(struct netdev *dev, uint32_t ip, uint8_t *mac_out)
{
    if (!mac_out) {
        return -1;
    }

    /* Check local cache */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
            memcpy(mac_out, s_arp_table[i].mac, ETH_ALEN);
            return 0;
        }
    }

    /* Send ARP Request */
    if (dev) {
        arp_send_request(dev, ip);

        /* Poll for reply */
        for (int iter = 0; iter < 100; iter++) {
            if (dev->ops && dev->ops->poll) {
                dev->ops->poll(dev);
            }
            for (int i = 0; i < ARP_TABLE_SIZE; i++) {
                if (s_arp_table[i].valid && s_arp_table[i].ip == ip) {
                    memcpy(mac_out, s_arp_table[i].mac, ETH_ALEN);
                    return 0;
                }
            }
            /* Short spin wait */
            for (volatile int d = 0; d < 10000; d++);
        }
    }

    return -1;
}

int arp_send_request(struct netdev *dev, uint32_t target_ip)
{
    if (!dev) {
        return -1;
    }

    netbuf_t *nb = netbuf_alloc(sizeof(struct arphdr));
    if (!nb) {
        return -1;
    }

    struct arphdr *arp = (struct arphdr *)netbuf_put(nb, sizeof(struct arphdr));
    if (!arp) {
        netbuf_free(nb);
        return -1;
    }

    arp->ar_hrd = net_htons(ARPHRD_ETHER);
    arp->ar_pro = net_htons(ETH_P_IP);
    arp->ar_hln = ETH_ALEN;
    arp->ar_pln = 4;
    arp->ar_op  = net_htons(ARPOP_REQUEST);

    memcpy(arp->ar_sha, dev->hwaddr, ETH_ALEN);
    arp->ar_sip = dev->ip_addr;
    memset(arp->ar_tha, 0x00, ETH_ALEN);
    arp->ar_tip = target_ip;

    return ethernet_send(dev, s_bcast_mac, ETH_P_ARP, nb);
}

int arp_send_reply(struct netdev *dev, const uint8_t *target_mac, uint32_t target_ip)
{
    if (!dev || !target_mac) {
        return -1;
    }

    netbuf_t *nb = netbuf_alloc(sizeof(struct arphdr));
    if (!nb) {
        return -1;
    }

    struct arphdr *arp = (struct arphdr *)netbuf_put(nb, sizeof(struct arphdr));
    if (!arp) {
        netbuf_free(nb);
        return -1;
    }

    arp->ar_hrd = net_htons(ARPHRD_ETHER);
    arp->ar_pro = net_htons(ETH_P_IP);
    arp->ar_hln = ETH_ALEN;
    arp->ar_pln = 4;
    arp->ar_op  = net_htons(ARPOP_REPLY);

    memcpy(arp->ar_sha, dev->hwaddr, ETH_ALEN);
    arp->ar_sip = dev->ip_addr;
    memcpy(arp->ar_tha, target_mac, ETH_ALEN);
    arp->ar_tip = target_ip;

    return ethernet_send(dev, target_mac, ETH_P_ARP, nb);
}

int arp_receive(struct netdev *dev, netbuf_t *nb)
{
    if (!dev || !nb || nb->len < sizeof(struct arphdr)) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct arphdr *arp = (struct arphdr *)nb->data;
    uint16_t op = net_ntohs(arp->ar_op);

    /* Update ARP cache from incoming packet */
    arp_cache_insert(arp->ar_sip, arp->ar_sha);

    /* Reply to requests targeted at our interface */
    if (op == ARPOP_REQUEST && arp->ar_tip == dev->ip_addr) {
        arp_send_reply(dev, arp->ar_sha, arp->ar_sip);
    }

    netbuf_free(nb);
    return 0;
}

const struct arp_entry *arp_get_table(void)
{
    return s_arp_table;
}
