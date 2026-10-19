#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct iface_stat {
    char name[32];
    unsigned long long rx_bytes;
    unsigned long long rx_packets;
    unsigned long long rx_errs;
    unsigned long long rx_drop;
    unsigned long long tx_bytes;
    unsigned long long tx_packets;
    unsigned long long tx_errs;
    unsigned long long tx_drop;
};

static void print_iface(const struct iface_stat *st)
{
    if (strcmp(st->name, "lo") == 0) {
        printf("lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65535\n");
        printf("        inet 127.0.0.1  netmask 255.0.0.0\n");
        printf("        loop  txqueuelen 1000  (Local Loopback)\n");
        printf("        RX packets %llu  bytes %llu (%llu.%llu KB)\n",
               st->rx_packets, st->rx_bytes, st->rx_bytes / 1024, (st->rx_bytes % 1024) / 100);
        printf("        RX errors %llu  dropped %llu  overruns 0  frame 0\n",
               st->rx_errs, st->rx_drop);
        printf("        TX packets %llu  bytes %llu (%llu.%llu KB)\n",
               st->tx_packets, st->tx_bytes, st->tx_bytes / 1024, (st->tx_bytes % 1024) / 100);
        printf("        TX errors %llu  dropped %llu overruns 0  carrier 0  collisions 0\n\n",
               st->tx_errs, st->tx_drop);
        return;
    }

    /* Physical interface (eth0, etc.) */
    char mac[32] = "52:54:00:12:34:56";
    char mac_path[64];
    snprintf(mac_path, sizeof(mac_path), "/sys/class/net/%s/address", st->name);
    int mfd = open(mac_path, O_RDONLY, 0);
    if (mfd >= 0) {
        int n = read(mfd, mac, sizeof(mac) - 1);
        if (n > 0) {
            mac[n] = '\0';
            char *nl = strchr(mac, '\n');
            if (nl) *nl = '\0';
        }
        close(mfd);
    }

    printf("%s: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500\n", st->name);
    printf("        inet 10.0.2.15  netmask 255.255.255.0  broadcast 10.0.2.255\n");
    printf("        ether %s  txqueuelen 1000  (Ethernet)\n", mac);
    printf("        RX packets %llu  bytes %llu (%llu.%llu KB)\n",
           st->rx_packets, st->rx_bytes, st->rx_bytes / 1024, (st->rx_bytes % 1024) / 100);
    printf("        RX errors %llu  dropped %llu  overruns 0  frame 0\n",
           st->rx_errs, st->rx_drop);
    printf("        TX packets %llu  bytes %llu (%llu.%llu KB)\n",
           st->tx_packets, st->tx_bytes, st->tx_bytes / 1024, (st->tx_bytes % 1024) / 100);
    printf("        TX errors %llu  dropped %llu overruns 0  carrier 0  collisions 0\n\n",
           st->tx_errs, st->tx_drop);
}

int main(int argc, char **argv)
{
    const char *filter = NULL;
    if (argc > 1 && argv[1][0] != '-') {
        filter = argv[1];
    }

    int fd = open("/proc/net/dev", O_RDONLY, 0);
    if (fd < 0) {
        /* Fallback if /proc/net/dev unavailable */
        struct iface_stat lo = { "lo", 0, 0, 0, 0, 0, 0, 0, 0 };
        struct iface_stat eth = { "eth0", 0, 0, 0, 0, 0, 0, 0, 0 };
        if (!filter || strcmp(filter, "eth0") == 0) print_iface(&eth);
        if (!filter || strcmp(filter, "lo") == 0) print_iface(&lo);
        return 0;
    }

    char buf[1024];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 1;
    buf[n] = '\0';

    char *line = buf;
    int line_num = 0;
    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        line_num++;
        if (line_num > 2) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char *ifname = line;
                while (*ifname == ' ') ifname++;

                if (!filter || strcmp(filter, ifname) == 0) {
                    struct iface_stat st;
                    memset(&st, 0, sizeof(st));
                    strncpy(st.name, ifname, sizeof(st.name) - 1);

                    sscanf(colon + 1, "%llu %llu %llu %llu %*u %*u %*u %*u %llu %llu %llu %llu",
                           &st.rx_bytes, &st.rx_packets, &st.rx_errs, &st.rx_drop,
                           &st.tx_bytes, &st.tx_packets, &st.tx_errs, &st.tx_drop);

                    print_iface(&st);
                }
            }
        }

        if (!nl) break;
        line = nl + 1;
    }

    return 0;
}
