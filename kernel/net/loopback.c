#include <aurora/net/netdev.h>
#include <aurora/string.h>

static struct netdev s_loopback_dev;

static int loopback_transmit(struct netdev *dev, netbuf_t *nb)
{
    if (!dev || !nb) {
        return -1;
    }

    dev->stats.tx_packets++;
    dev->stats.tx_bytes += nb->len;

    /* Loop back directly into netdev_receive */
    return netdev_receive(dev, nb);
}

static struct netdev_ops s_loopback_ops = {
    .init = NULL,
    .transmit = loopback_transmit,
    .poll = NULL
};

void loopback_init(void)
{
    memset(&s_loopback_dev, 0, sizeof(s_loopback_dev));
    strncpy(s_loopback_dev.name, "lo", sizeof(s_loopback_dev.name) - 1);

    /* 127.0.0.1 in network byte order */
    s_loopback_dev.ip_addr = net_htonl(0x7F000001);
    /* 255.0.0.0 in network byte order */
    s_loopback_dev.netmask = net_htonl(0xFF000000);
    s_loopback_dev.gateway = 0;

    s_loopback_dev.flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING;
    s_loopback_dev.mtu = 65535;
    s_loopback_dev.ops = &s_loopback_ops;

    netdev_register(&s_loopback_dev);
}
