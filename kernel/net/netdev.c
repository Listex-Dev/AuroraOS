#include <aurora/net/netdev.h>
#include <aurora/net/ethernet.h>
#include <aurora/net/ipv4.h>
#include <aurora/fs/sysfs.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static struct netdev *s_netdev_list = NULL;

void netdev_subsys_init(void)
{
    s_netdev_list = NULL;
    loopback_init();
    printk("<6>[  0.000000] Net: Network device subsystem initialized\n");
}

int netdev_register(struct netdev *dev)
{
    if (!dev) {
        return -1;
    }

    dev->next = s_netdev_list;
    s_netdev_list = dev;

    sysfs_register_netdev(dev);

    printk("<6>[NET] Registered network interface '%s' (flags 0x%x, MTU %u)\n",
           dev->name, dev->flags, dev->mtu);
    return 0;
}

struct netdev *netdev_get_by_name(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (struct netdev *cur = s_netdev_list; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) {
            return cur;
        }
    }
    return NULL;
}

struct netdev *netdev_get_default(void)
{
    /* Prefer eth0 or any non-loopback UP interface */
    for (struct netdev *cur = s_netdev_list; cur != NULL; cur = cur->next) {
        if ((cur->flags & IFF_UP) && !(cur->flags & IFF_LOOPBACK)) {
            return cur;
        }
    }
    /* Fall back to loopback or first device */
    return s_netdev_list;
}

struct netdev *netdev_get_list(void)
{
    return s_netdev_list;
}

int netdev_receive(struct netdev *dev, netbuf_t *nb)
{
    if (!dev || !nb) {
        return -1;
    }

    dev->stats.rx_packets++;
    dev->stats.rx_bytes += nb->len;

    if (dev->flags & IFF_LOOPBACK) {
        return ipv4_receive(dev, nb);
    }

    return ethernet_receive(dev, nb);
}
