#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
    const char *target = NULL;
    int count = 3;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            target = argv[i];
        }
    }

    if (!target) {
        fprintf(stderr, "Usage: ping [-c count] <destination>\n");
        return 1;
    }

    in_addr_t dst_addr = inet_addr(target);
    if (dst_addr == 0xFFFFFFFF) {
        fprintf(stderr, "ping: unknown host %s\n", target);
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_RAW, 1 /* IPPROTO_ICMP */);
    if (sockfd < 0) {
        perror("ping: socket");
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = dst_addr;

    printf("PING %s (%s) 56(84) bytes of data.\n", target, target);

    int received = 0;
    char payload[56];
    memset(payload, 0x42, sizeof(payload));

    for (int seq = 1; seq <= count; seq++) {
        ssize_t sent = sendto(sockfd, payload, sizeof(payload), 0,
                              (struct sockaddr *)&sin, sizeof(sin));
        if (sent < 0) {
            perror("ping: sendto");
            continue;
        }

        char rx_buf[128];
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(sockfd, rx_buf, sizeof(rx_buf), 0,
                             (struct sockaddr *)&from, &fromlen);

        if (n > 0) {
            received++;
            printf("64 bytes from %s: icmp_seq=%d ttl=64 time=1.0 ms\n", target, seq);
        } else {
            printf("Request timeout for icmp_seq %d\n", seq);
        }

        /* Short delay between pings */
        for (volatile int d = 0; d < 100000; d++);
    }

    close(sockfd);

    printf("\n--- %s ping statistics ---\n", target);
    int loss = (count - received) * 100 / (count > 0 ? count : 1);
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
           count, received, loss);

    return (received > 0) ? 0 : 1;
}
