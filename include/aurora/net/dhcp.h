#ifndef _AURORA_NET_DHCP_H
#define _AURORA_NET_DHCP_H

#include <aurora/types.h>
#include <aurora/net/netdev.h>

#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68
#define DHCP_MAGIC_COOKIE   0x63825363

#define DHCP_BOOTREQUEST    1
#define DHCP_BOOTREPLY      2

#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6

#define DHCP_OPT_PAD          0
#define DHCP_OPT_SUBNET_MASK  1
#define DHCP_OPT_ROUTER       3
#define DHCP_OPT_DNS          6
#define DHCP_OPT_HOST_NAME    12
#define DHCP_OPT_DOMAIN_NAME  15
#define DHCP_OPT_REQ_IP       50
#define DHCP_OPT_LEASE_TIME   51
#define DHCP_OPT_MSG_TYPE     53
#define DHCP_OPT_SERVER_ID    54
#define DHCP_OPT_PARAM_REQ    55
#define DHCP_OPT_END          255

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7

struct dhcp_packet {
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

struct dhcp_lease {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint32_t server_id;
    uint32_t lease_time;
};

int dhcp_configure_interface(struct netdev *dev, struct dhcp_lease *out_lease);
const struct dhcp_lease *dhcp_get_active_lease(void);

#endif /* _AURORA_NET_DHCP_H */
