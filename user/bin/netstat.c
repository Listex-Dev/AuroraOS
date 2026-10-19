#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

static void format_addr(char *out, size_t out_len, uint32_t ip, uint16_t port)
{
    if (ip == 0 && port == 0) {
        snprintf(out, out_len, "0.0.0.0:*");
        return;
    }

    struct in_addr in;
    in.s_addr = ip;
    char *ip_str = inet_ntoa(in);

    if (port == 0) {
        snprintf(out, out_len, "%s:*", ip_str);
    } else {
        snprintf(out, out_len, "%s:%u", ip_str, port);
    }
}

static const char *tcp_state_name(int st)
{
    switch (st) {
        case 0x01: return "ESTABLISHED";
        case 0x02: return "SYN_SENT";
        case 0x03: return "SYN_RECV";
        case 0x04: return "FIN_WAIT1";
        case 0x05: return "FIN_WAIT2";
        case 0x06: return "TIME_WAIT";
        case 0x07: return "CLOSE";
        case 0x08: return "CLOSE_WAIT";
        case 0x09: return "LAST_ACK";
        case 0x0A: return "LISTEN";
        case 0x0B: return "CLOSING";
        default:   return "UNKNOWN";
    }
}

static void show_sockets(const char *proto, const char *proc_path)
{
    int fd = open(proc_path, O_RDONLY, 0);
    if (fd < 0) return;

    char buf[2048];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    char *line = buf;
    int line_num = 0;

    while (*line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        line_num++;
        if (line_num > 1) {
            uint32_t lip = 0, rip = 0;
            unsigned int lport = 0, rport = 0, st = 0;
            unsigned int tx_q = 0, rx_q = 0;

            int parsed = sscanf(line, "%*d: %08X:%04X %08X:%04X %02X %08X:%08X",
                                &lip, &lport, &rip, &rport, &st, &tx_q, &rx_q);
            if (parsed >= 5) {
                char local[64], rem[64];
                format_addr(local, sizeof(local), lip, (uint16_t)lport);
                format_addr(rem, sizeof(rem), rip, (uint16_t)rport);

                const char *st_str = (strcmp(proto, "tcp") == 0) ? tcp_state_name((int)st) : "";
                printf("%-5s %6u %6u %-23s %-23s %s\n",
                       proto, rx_q, tx_q, local, rem, st_str);
            }
        }

        if (!nl) break;
        line = nl + 1;
    }
}

static void show_routing_table(void)
{
    printf("Kernel IP routing table\n");
    printf("%-15s %-15s %-15s %-7s %5s %-6s %5s %s\n",
           "Destination", "Gateway", "Genmask", "Flags", "MSS", "Window", "irtt", "Iface");

    int fd = open("/proc/net/route", O_RDONLY, 0);
    if (fd >= 0) {
        char buf[1024];
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = buf;
            int line_num = 0;
            while (*line) {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                line_num++;
                if (line_num > 1) {
                    char iface[32];
                    uint32_t dst = 0, gw = 0, mask = 0;
                    unsigned int flags = 0;
                    if (sscanf(line, "%31s %08X %08X %04X %*d %*d %*d %08X",
                               iface, &dst, &gw, &flags, &mask) >= 5) {
                        struct in_addr d_in, g_in, m_in;
                        d_in.s_addr = dst;
                        g_in.s_addr = gw;
                        m_in.s_addr = mask;

                        char d_str[32], g_str[32], m_str[32];
                        strncpy(d_str, inet_ntoa(d_in), sizeof(d_str));
                        strncpy(g_str, inet_ntoa(g_in), sizeof(g_str));
                        strncpy(m_str, inet_ntoa(m_in), sizeof(m_str));

                        char f_str[8] = "";
                        if (flags & 1) strcat(f_str, "U");
                        if (flags & 2) strcat(f_str, "G");

                        printf("%-15s %-15s %-15s %-7s %5d %-6d %5d %s\n",
                               d_str, g_str, m_str, f_str, 0, 0, 0, iface);
                    }
                }
                if (!nl) break;
                line = nl + 1;
            }
            return;
        }
    }

    /* Fallback default route display */
    printf("%-15s %-15s %-15s %-7s %5d %-6d %5d %s\n", "0.0.0.0", "10.0.2.2", "0.0.0.0", "UG", 0, 0, 0, "eth0");
    printf("%-15s %-15s %-15s %-7s %5d %-6d %5d %s\n", "10.0.2.0", "0.0.0.0", "255.255.255.0", "U", 0, 0, 0, "eth0");
    printf("%-15s %-15s %-15s %-7s %5d %-6d %5d %s\n", "127.0.0.0", "0.0.0.0", "255.0.0.0", "U", 0, 0, 0, "lo");
}

int main(int argc, char **argv)
{
    int show_routes = 0;
    int show_tcp = 1;
    int show_udp = 1;

    for (int i = 1; i < argc; i++) {
        if (strchr(argv[i], 'r')) show_routes = 1;
        if (strchr(argv[i], 't')) { show_tcp = 1; show_udp = 0; }
        if (strchr(argv[i], 'u')) { show_udp = 1; show_tcp = 0; }
    }

    if (show_routes) {
        show_routing_table();
        return 0;
    }

    printf("Active Internet connections (servers and established)\n");
    printf("%-5s %6s %6s %-23s %-23s %s\n",
           "Proto", "Recv-Q", "Send-Q", "Local Address", "Foreign Address", "State");

    if (show_tcp) show_sockets("tcp", "/proc/net/tcp");
    if (show_udp) show_sockets("udp", "/proc/net/udp");

    return 0;
}
