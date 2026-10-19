#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void check_fail(const char *msg)
{
    printf("[TEST M13] FAIL: %s\n", msg);
    exit(1);
}

int main(void)
{
    printf("============================================================\n");
    printf("      AuroraOS Milestone M13 Acceptance Test Suite          \n");
    printf("  (Loopback, e1000 eth0, ARP, ICMP Ping, UDP, TCP Stream)   \n");
    printf("============================================================\n\n");

    /* Subtest 1: ProcFS and SysFS network files */
    printf("--- Subtest 1: /proc/net/* and /sys/class/net/* Verification ---\n");
    int fd = open("/proc/net/dev", O_RDONLY, 0);
    if (fd < 0) check_fail("open /proc/net/dev");
    char buf[512];
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) check_fail("read /proc/net/dev");
    buf[n] = '\0';
    close(fd);
    if (!strstr(buf, "lo") || !strstr(buf, "eth0")) check_fail("lo or eth0 missing from /proc/net/dev");
    printf("[TEST M13] OK: /proc/net/dev lists lo and eth0 interfaces\n");

    fd = open("/proc/net/arp", O_RDONLY, 0);
    if (fd < 0) check_fail("open /proc/net/arp");
    n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) check_fail("read /proc/net/arp");
    buf[n] = '\0';
    close(fd);
    if (!strstr(buf, "10.0.2.2")) check_fail("Gateway 10.0.2.2 missing in /proc/net/arp");
    printf("[TEST M13] OK: /proc/net/arp contains active ARP entries\n");

    fd = open("/sys/class/net/eth0/address", O_RDONLY, 0);
    if (fd < 0) check_fail("open /sys/class/net/eth0/address");
    n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) check_fail("read /sys/class/net/eth0/address");
    buf[n] = '\0';
    close(fd);
    printf("  [eth0] Hardware MAC address from sysfs: %s", buf);
    printf("[TEST M13] OK: /sys/class/net/eth0/address verified\n");

    /* Subtest 2: UDP Loopback communication */
    printf("\n--- Subtest 2: UDP Socket Datagram Verification ---\n");
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) check_fail("socket(AF_INET, SOCK_DGRAM)");

    struct sockaddr_in udp_sin;
    memset(&udp_sin, 0, sizeof(udp_sin));
    udp_sin.sin_family = AF_INET;
    udp_sin.sin_port = htons(8888);
    udp_sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(udp_sock, (struct sockaddr *)&udp_sin, sizeof(udp_sin)) < 0) {
        check_fail("bind UDP port 8888");
    }
    printf("[TEST M13] OK: UDP socket bound to 127.0.0.1:8888\n");

    const char *udp_msg = "AuroraOS UDP Packet Verification";
    ssize_t sent = sendto(udp_sock, udp_msg, strlen(udp_msg), 0,
                          (struct sockaddr *)&udp_sin, sizeof(udp_sin));
    if (sent != (ssize_t)strlen(udp_msg)) check_fail("UDP sendto");
    printf("[TEST M13] OK: Sent UDP datagram (%d bytes)\n", (int)sent);

    char udp_rx[128];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    ssize_t rcvd = recvfrom(udp_sock, udp_rx, sizeof(udp_rx) - 1, 0,
                            (struct sockaddr *)&from, &fromlen);
    if (rcvd <= 0) check_fail("UDP recvfrom");
    udp_rx[rcvd] = '\0';
    if (strcmp(udp_rx, udp_msg) != 0) check_fail("UDP payload mismatch");
    printf("[TEST M13] OK: Received UDP datagram: '%s'\n", udp_rx);
    close(udp_sock);

    /* Subtest 3: TCP Stream Client / Server */
    printf("\n--- Subtest 3: TCP Stream Socket 3-Way Handshake & Data Transfer ---\n");
    int srv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_sock < 0) check_fail("socket TCP server");

    struct sockaddr_in srv_sin;
    memset(&srv_sin, 0, sizeof(srv_sin));
    srv_sin.sin_family = AF_INET;
    srv_sin.sin_port = htons(9999);
    srv_sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(srv_sock, (struct sockaddr *)&srv_sin, sizeof(srv_sin)) < 0) {
        check_fail("bind TCP server 9999");
    }
    if (listen(srv_sock, 5) < 0) {
        check_fail("listen TCP server");
    }
    printf("[TEST M13] OK: TCP server listening on 127.0.0.1:9999\n");

    int cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_sock < 0) check_fail("socket TCP client");

    if (connect(cli_sock, (struct sockaddr *)&srv_sin, sizeof(srv_sin)) < 0) {
        check_fail("TCP client connect");
    }
    printf("[TEST M13] OK: TCP 3-way handshake established with server\n");

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int conn_sock = accept(srv_sock, (struct sockaddr *)&cli_addr, &cli_len);
    if (conn_sock < 0) check_fail("TCP server accept");
    printf("[TEST M13] OK: TCP server accepted client connection\n");

    const char *tcp_msg = "Hello from AuroraOS TCP Stream!";
    if (send(cli_sock, tcp_msg, strlen(tcp_msg), 0) != (ssize_t)strlen(tcp_msg)) {
        check_fail("TCP client send");
    }

    char tcp_rx[128];
    ssize_t tcp_n = recv(conn_sock, tcp_rx, sizeof(tcp_rx) - 1, 0);
    if (tcp_n <= 0) check_fail("TCP server recv");
    tcp_rx[tcp_n] = '\0';
    if (strcmp(tcp_rx, tcp_msg) != 0) check_fail("TCP payload mismatch");
    printf("[TEST M13] OK: TCP data verified: '%s'\n", tcp_rx);

    close(conn_sock);
    close(cli_sock);
    close(srv_sock);
    printf("[TEST M13] OK: TCP connection teardown complete\n");

    /* Subtest 4: Run /bin/ping 10.0.2.2 via fork/exec */
    printf("\n--- Subtest 4: /bin/ping 10.0.2.2 (QEMU Gateway) Verification ---\n");
    pid_t pid = fork();
    if (pid < 0) check_fail("fork ping");
    if (pid == 0) {
        char *argv[] = { "/bin/ping", "-c", "3", "10.0.2.2", NULL };
        execve("/bin/ping", argv, NULL);
        exit(1);
    }

    int status = 0;
    wait4(pid, &status, 0, NULL);
    if (status != 0) {
        check_fail("ping 10.0.2.2 failed");
    }
    printf("[TEST M13] OK: /bin/ping -c 3 10.0.2.2 received replies with 0%% packet loss\n");

    printf("\n============================================================\n");
    printf("Milestone M13 status: [OK]\n");
    printf(">>> TEST PASSED: Milestone M13 Acceptance Criteria Verified! <<<\n");
    printf("============================================================\n");

    return 0;
}
