#include <aurora/net/icmp.h>
#include <aurora/net/ipv4.h>
#include <aurora/net/netdev.h>
#include <aurora/printk.h>
#include <aurora/string.h>

#define MAX_ICMP_WAITERS 16

struct icmp_waiter {
    uint16_t id;
    uint16_t seq;
    bool active;
    bool received;
};

static struct icmp_waiter s_waiters[MAX_ICMP_WAITERS];

void icmp_init(void)
{
    memset(s_waiters, 0, sizeof(s_waiters));
    printk("<6>[  0.000000] Net: ICMP subsystem initialized\n");
}

int icmp_receive(struct netdev *dev, uint32_t src_ip, netbuf_t *nb)
{
    if (!dev || !nb || nb->len < sizeof(struct icmphdr)) {
        if (nb) netbuf_free(nb);
        return -1;
    }

    struct icmphdr *icmp = (struct icmphdr *)nb->data;

    if (icmp->type == ICMP_ECHO) {
        /* Echo Request -> Construct Echo Reply */
        icmp->type = ICMP_ECHOREPLY;
        icmp->code = 0;
        icmp->checksum = 0;
        icmp->checksum = ip_checksum(icmp, nb->len);

        /* Send back to requester */
        return ipv4_send(dev, src_ip, IPPROTO_ICMP, nb);
    } else if (icmp->type == ICMP_ECHOREPLY) {
        uint16_t id = net_ntohs(icmp->un.echo.id);
        uint16_t seq = net_ntohs(icmp->un.echo.sequence);

        for (int i = 0; i < MAX_ICMP_WAITERS; i++) {
            if (s_waiters[i].active && s_waiters[i].id == id && s_waiters[i].seq == seq) {
                s_waiters[i].received = true;
                break;
            }
        }
        netbuf_free(nb);
        return 0;
    }

    netbuf_free(nb);
    return 0;
}

int icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq, const void *data, size_t len)
{
    size_t total_len = sizeof(struct icmphdr) + len;
    netbuf_t *nb = netbuf_alloc(total_len);
    if (!nb) {
        return -1;
    }

    struct icmphdr *icmp = (struct icmphdr *)netbuf_put(nb, total_len);
    if (!icmp) {
        netbuf_free(nb);
        return -1;
    }

    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->un.echo.id = net_htons(id);
    icmp->un.echo.sequence = net_htons(seq);

    if (data && len > 0) {
        memcpy((uint8_t *)icmp + sizeof(struct icmphdr), data, len);
    }

    icmp->checksum = ip_checksum(icmp, total_len);

    /* Register waiter */
    int slot = -1;
    for (int i = 0; i < MAX_ICMP_WAITERS; i++) {
        if (!s_waiters[i].active) {
            slot = i;
            break;
        }
    }
    if (slot >= 0) {
        s_waiters[slot].id = id;
        s_waiters[slot].seq = seq;
        s_waiters[slot].active = true;
        s_waiters[slot].received = false;
    }

    int ret = ipv4_send(NULL, dst_ip, IPPROTO_ICMP, nb);
    if (ret < 0 && slot >= 0) {
        s_waiters[slot].active = false;
    }
    return ret;
}

#include <aurora/arch/x86_64/pit.h>

int icmp_wait_reply(uint16_t id, uint16_t seq, uint32_t timeout_ms)
{
    int slot = -1;
    for (int i = 0; i < MAX_ICMP_WAITERS; i++) {
        if (s_waiters[i].active && s_waiters[i].id == id && s_waiters[i].seq == seq) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return -1;
    }

    /* Poll network device */
    struct netdev *dev = netdev_get_default();
    uint64_t start_tick = pit_get_ticks();
    uint64_t max_ticks = (timeout_ms / 10) + 2;

    while (pit_get_ticks() - start_tick < max_ticks) {
        if (s_waiters[slot].received) {
            s_waiters[slot].active = false;
            return 0;
        }

        if (dev && dev->ops && dev->ops->poll) {
            dev->ops->poll(dev);
        }

        if (s_waiters[slot].received) {
            s_waiters[slot].active = false;
            return 0;
        }

        __asm__ volatile ("sti; hlt");
    }

    s_waiters[slot].active = false;
    return -1; /* Timeout */
}
