#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: nslookup <host>\n");
        return 1;
    }

    const char *host = argv[1];

    /* Read nameserver from /etc/resolv.conf */
    char ns[64] = "10.0.2.3";
    int fd = open("/etc/resolv.conf", O_RDONLY, 0);
    if (fd >= 0) {
        char buf[256];
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            char *p = strstr(buf, "nameserver");
            if (p) {
                p += 10;
                while (*p == ' ' || *p == '\t') p++;
                char *end = p;
                while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') end++;
                size_t len = (size_t)(end - p);
                if (len > 0 && len < sizeof(ns)) {
                    memcpy(ns, p, len);
                    ns[len] = '\0';
                }
            }
        }
        close(fd);
    }

    printf("Server:\t\t%s\n", ns);
    printf("Address:\t%s#53\n\n", ns);

    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) {
        fprintf(stderr, "** server can't find %s: NXDOMAIN\n", host);
        return 1;
    }

    struct in_addr in;
    memcpy(&in.s_addr, he->h_addr_list[0], 4);

    printf("Non-authoritative answer:\n");
    printf("Name:\t%s\n", host);
    printf("Address: %s\n", inet_ntoa(in));

    return 0;
}
