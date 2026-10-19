#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int h_errno = 0;

static struct hostent s_hostent;
static char s_host_name[256];
static char *s_aliases[1] = { NULL };
static uint32_t s_addr_entry;
static char *s_addr_list[2] = { (char *)&s_addr_entry, NULL };

struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

static int encode_qname(char *dst, size_t dst_len, const char *src)
{
    size_t src_len = strlen(src);
    if (src_len + 2 > dst_len) return -1;

    size_t out_idx = 0;
    const char *start = src;
    const char *dot;

    while ((dot = strchr(start, '.')) != NULL) {
        size_t label_len = (size_t)(dot - start);
        if (label_len == 0 || label_len > 63) return -1;
        dst[out_idx++] = (char)label_len;
        memcpy(dst + out_idx, start, label_len);
        out_idx += label_len;
        start = dot + 1;
    }

    size_t rem = strlen(start);
    if (rem > 0) {
        if (rem > 63) return -1;
        dst[out_idx++] = (char)rem;
        memcpy(dst + out_idx, start, rem);
        out_idx += rem;
    }

    dst[out_idx++] = 0;
    return (int)out_idx;
}

static int parse_resolv_conf(char *ns_ip, size_t max_len)
{
    int fd = open("/etc/resolv.conf", O_RDONLY, 0);
    if (fd < 0) {
        strncpy(ns_ip, "10.0.2.3", max_len - 1);
        ns_ip[max_len - 1] = '\0';
        return 0;
    }

    char buf[512];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        strncpy(ns_ip, "10.0.2.3", max_len - 1);
        ns_ip[max_len - 1] = '\0';
        return 0;
    }
    buf[n] = '\0';

    char *line = buf;
    while (*line) {
        while (*line == ' ' || *line == '\t') line++;
        if (strncmp(line, "nameserver", 10) == 0 && (line[10] == ' ' || line[10] == '\t')) {
            char *p = line + 10;
            while (*p == ' ' || *p == '\t') p++;
            char *end = p;
            while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') end++;
            size_t ip_len = (size_t)(end - p);
            if (ip_len > 0 && ip_len < max_len) {
                memcpy(ns_ip, p, ip_len);
                ns_ip[ip_len] = '\0';
                return 0;
            }
        }
        char *nl = strchr(line, '\n');
        if (!nl) break;
        line = nl + 1;
    }

    strncpy(ns_ip, "10.0.2.3", max_len - 1);
    ns_ip[max_len - 1] = '\0';
    return 0;
}

static int check_etc_hosts(const char *name, uint32_t *ip_out)
{
    int fd = open("/etc/hosts", O_RDONLY, 0);
    if (fd < 0) return -1;

    char buf[1024];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';

    char *line = buf;
    while (*line) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '#' || *line == '\r' || *line == '\n') {
            char *nl = strchr(line, '\n');
            if (!nl) break;
            line = nl + 1;
            continue;
        }

        /* Extract IP */
        char *ip_start = line;
        while (*line && *line != ' ' && *line != '\t' && *line != '\n') line++;
        char ip_str[64];
        size_t ip_len = (size_t)(line - ip_start);
        if (ip_len >= sizeof(ip_str)) {
            char *nl = strchr(line, '\n');
            if (!nl) break;
            line = nl + 1;
            continue;
        }
        memcpy(ip_str, ip_start, ip_len);
        ip_str[ip_len] = '\0';

        /* Scan hosts */
        while (*line && *line != '\n') {
            while (*line == ' ' || *line == '\t') line++;
            if (*line == '#' || *line == '\n' || *line == '\r') break;

            char *h_start = line;
            while (*line && *line != ' ' && *line != '\t' && *line != '\r' && *line != '\n') line++;
            char h_name[64];
            size_t h_len = (size_t)(line - h_start);
            if (h_len < sizeof(h_name)) {
                memcpy(h_name, h_start, h_len);
                h_name[h_len] = '\0';
                if (strcmp(name, h_name) == 0) {
                    in_addr_t addr = inet_addr(ip_str);
                    if (addr != (in_addr_t)-1) {
                        *ip_out = addr;
                        return 0;
                    }
                }
            }
        }

        char *nl = strchr(line, '\n');
        if (!nl) break;
        line = nl + 1;
    }

    return -1;
}

struct hostent *gethostbyname(const char *name)
{
    if (!name || strlen(name) == 0) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    /* 1. If name is already a numeric IPv4 address */
    in_addr_t num_addr = inet_addr(name);
    if (num_addr != (in_addr_t)-1 || strcmp(name, "255.255.255.255") == 0) {
        strncpy(s_host_name, name, sizeof(s_host_name) - 1);
        s_host_name[sizeof(s_host_name) - 1] = '\0';
        s_addr_entry = num_addr;
        s_hostent.h_name = s_host_name;
        s_hostent.h_aliases = s_aliases;
        s_hostent.h_addrtype = AF_INET;
        s_hostent.h_length = 4;
        s_hostent.h_addr_list = s_addr_list;
        return &s_hostent;
    }

    /* 2. Check /etc/hosts */
    uint32_t hosts_ip = 0;
    if (check_etc_hosts(name, &hosts_ip) == 0) {
        strncpy(s_host_name, name, sizeof(s_host_name) - 1);
        s_host_name[sizeof(s_host_name) - 1] = '\0';
        s_addr_entry = hosts_ip;
        s_hostent.h_name = s_host_name;
        s_hostent.h_aliases = s_aliases;
        s_hostent.h_addrtype = AF_INET;
        s_hostent.h_length = 4;
        s_hostent.h_addr_list = s_addr_list;
        return &s_hostent;
    }

    /* 3. Determine nameserver from /etc/resolv.conf */
    char ns_ip[64];
    parse_resolv_conf(ns_ip, sizeof(ns_ip));

    /* 4. Perform RFC 1035 UDP DNS Query */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        h_errno = NO_RECOVERY;
        return NULL;
    }

    struct sockaddr_in dns_addr;
    memset(&dns_addr, 0, sizeof(dns_addr));
    dns_addr.sin_family = AF_INET;
    dns_addr.sin_port = htons(53);
    dns_addr.sin_addr.s_addr = inet_addr(ns_ip);

    uint8_t packet[512];
    memset(packet, 0, sizeof(packet));

    struct dns_header *hdr = (struct dns_header *)packet;
    uint16_t qid = 0x5432;
    hdr->id = htons(qid);
    hdr->flags = htons(0x0100); /* RD=1 standard query */
    hdr->qdcount = htons(1);

    char qname_buf[256];
    int qname_len = encode_qname(qname_buf, sizeof(qname_buf), name);
    if (qname_len < 0) {
        close(sock);
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    uint8_t *ptr = packet + sizeof(struct dns_header);
    memcpy(ptr, qname_buf, (size_t)qname_len);
    ptr += qname_len;

    /* QTYPE = 1 (A), QCLASS = 1 (IN) */
    *(uint16_t *)ptr = htons(1); ptr += 2;
    *(uint16_t *)ptr = htons(1); ptr += 2;

    size_t query_len = (size_t)(ptr - packet);

    ssize_t sent = sendto(sock, packet, query_len, 0,
                          (struct sockaddr *)&dns_addr, sizeof(dns_addr));
    if (sent != (ssize_t)query_len) {
        close(sock);
        h_errno = TRY_AGAIN;
        return NULL;
    }

    /* Wait for DNS response */
    uint8_t resp_buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t rcvd = -1;

    for (int iter = 0; iter < 50; iter++) {
        rcvd = recvfrom(sock, resp_buf, sizeof(resp_buf), 0,
                        (struct sockaddr *)&from, &fromlen);
        if (rcvd >= (ssize_t)sizeof(struct dns_header)) {
            break;
        }
        usleep(10000);
    }

    close(sock);

    if (rcvd < (ssize_t)sizeof(struct dns_header)) {
        /* Fallback for standard local network names if nameserver didn't answer */
        if (strcmp(name, "localhost") == 0 || strcmp(name, "aurora") == 0) {
            s_addr_entry = inet_addr("127.0.0.1");
        } else if (strcmp(name, "gateway") == 0 || strcmp(name, "router") == 0) {
            s_addr_entry = inet_addr("10.0.2.2");
        } else {
            h_errno = HOST_NOT_FOUND;
            return NULL;
        }

        strncpy(s_host_name, name, sizeof(s_host_name) - 1);
        s_host_name[sizeof(s_host_name) - 1] = '\0';
        s_hostent.h_name = s_host_name;
        s_hostent.h_aliases = s_aliases;
        s_hostent.h_addrtype = AF_INET;
        s_hostent.h_length = 4;
        s_hostent.h_addr_list = s_addr_list;
        return &s_hostent;
    }

    struct dns_header *r_hdr = (struct dns_header *)resp_buf;
    if (ntohs(r_hdr->id) != qid) {
        h_errno = TRY_AGAIN;
        return NULL;
    }

    uint16_t ancount = ntohs(r_hdr->ancount);
    if (ancount == 0) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    /* Skip Question section */
    uint8_t *r_ptr = resp_buf + sizeof(struct dns_header);
    uint8_t *r_end = resp_buf + rcvd;

    while (r_ptr < r_end && *r_ptr != 0) {
        if ((*r_ptr & 0xC0) == 0xC0) {
            r_ptr += 2;
            break;
        }
        r_ptr += (1 + *r_ptr);
    }
    if (r_ptr < r_end && *r_ptr == 0) r_ptr++;
    r_ptr += 4; /* Skip QTYPE and QCLASS */

    /* Parse Answers */
    for (int a = 0; a < ancount && r_ptr < r_end; a++) {
        /* Skip Name */
        if ((*r_ptr & 0xC0) == 0xC0) {
            r_ptr += 2;
        } else {
            while (r_ptr < r_end && *r_ptr != 0) {
                r_ptr += (1 + *r_ptr);
            }
            if (r_ptr < r_end && *r_ptr == 0) r_ptr++;
        }

        if (r_ptr + 10 > r_end) break;
        uint16_t type = ntohs(*(uint16_t *)r_ptr); r_ptr += 2;
        /* uint16_t class = ntohs(*(uint16_t *)r_ptr); */ r_ptr += 2;
        /* uint32_t ttl = ntohl(*(uint32_t *)r_ptr); */ r_ptr += 4;
        uint16_t rdlen = ntohs(*(uint16_t *)r_ptr); r_ptr += 2;

        if (type == 1 && rdlen == 4 && r_ptr + 4 <= r_end) {
            /* IPv4 A Record found */
            memcpy(&s_addr_entry, r_ptr, 4);

            strncpy(s_host_name, name, sizeof(s_host_name) - 1);
            s_host_name[sizeof(s_host_name) - 1] = '\0';
            s_hostent.h_name = s_host_name;
            s_hostent.h_aliases = s_aliases;
            s_hostent.h_addrtype = AF_INET;
            s_hostent.h_length = 4;
            s_hostent.h_addr_list = s_addr_list;
            return &s_hostent;
        }
        r_ptr += rdlen;
    }

    h_errno = HOST_NOT_FOUND;
    return NULL;
}

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type)
{
    if (!addr || len != 4 || type != AF_INET) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    memcpy(&s_addr_entry, addr, 4);
    struct in_addr in;
    in.s_addr = s_addr_entry;
    char *ip_str = inet_ntoa(in);
    if (ip_str) {
        strncpy(s_host_name, ip_str, sizeof(s_host_name) - 1);
        s_host_name[sizeof(s_host_name) - 1] = '\0';
    }

    s_hostent.h_name = s_host_name;
    s_hostent.h_aliases = s_aliases;
    s_hostent.h_addrtype = AF_INET;
    s_hostent.h_length = 4;
    s_hostent.h_addr_list = s_addr_list;
    return &s_hostent;
}
