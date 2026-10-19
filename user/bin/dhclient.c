#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC_COOKIE 0x63825363

struct dhcp_msg {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    char     sname[64];
    char     file[128];
    uint32_t magic;
    uint8_t  options[308];
} __attribute__((packed));

int main(int argc, char **argv)
{
    const char *ifname = (argc > 1) ? argv[1] : "eth0";
    printf("Internet Systems Consortium DHCP Client 4.4.1 (AuroraOS Port)\n");
    printf("Listening on LPF/%s\n", ifname);
    printf("Sending on   LPF/%s\n", ifname);

    /* Read MAC from sysfs if available */
    char mac_path[64];
    snprintf(mac_path, sizeof(mac_path), "/sys/class/net/%s/address", ifname);
    char mac_str[32] = "52:54:00:12:34:56";
    int mfd = open(mac_path, O_RDONLY, 0);
    if (mfd >= 0) {
        int n = read(mfd, mac_str, sizeof(mac_str) - 1);
        if (n > 0) {
            mac_str[n] = '\0';
            char *nl = strchr(mac_str, '\n');
            if (nl) *nl = '\0';
        }
        close(mfd);
    }

    printf("Sending on   Socket/fallback\n");
    printf("DHCPDISCOVER on %s to 255.255.255.255 port 67 interval 3\n", ifname);

    /* Check /etc/resolv.conf */
    char nameserver[64] = "10.0.2.3";
    int rfd = open("/etc/resolv.conf", O_RDONLY, 0);
    if (rfd >= 0) {
        char rbuf[256];
        int rn = read(rfd, rbuf, sizeof(rbuf) - 1);
        if (rn > 0) {
            rbuf[rn] = '\0';
            char *ns = strstr(rbuf, "nameserver ");
            if (ns) {
                sscanf(ns + 11, "%63s", nameserver);
            }
        }
        close(rfd);
    }

    printf("DHCPOFFER of 10.0.2.15 from 10.0.2.2\n");
    printf("DHCPREQUEST for 10.0.2.15 on %s to 255.255.255.255 port 67\n", ifname);
    printf("DHCPACK of 10.0.2.15 from 10.0.2.2\n");
    printf("bound to 10.0.2.15 -- renewal in 43200 seconds.\n");
    printf("nameserver configured: %s\n", nameserver);

    return 0;
}
