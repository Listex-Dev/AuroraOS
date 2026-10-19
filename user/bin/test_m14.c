#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void check_fail(const char *msg)
{
    printf("[TEST M14] FAIL: %s\n", msg);
    exit(1);
}

int main(void)
{
    printf("============================================================\n");
    printf("      AuroraOS Milestone M14 Acceptance Test Suite          \n");
    printf("  (Unix98 PTY, DHCP Client, DNS Resolver, ifconfig, netstat, wget)\n");
    printf("============================================================\n\n");

    /* Subtest 1: PTY Subsystem Verification */
    printf("--- Subtest 1: Unix98 Pseudo-Terminal (PTY) Subsystem ---\n");
    int master_fd = -1;
    int slave_fd = -1;
    char pty_name[64];
    memset(pty_name, 0, sizeof(pty_name));

    if (openpty(&master_fd, &slave_fd, pty_name, NULL, NULL) < 0) {
        check_fail("openpty() failed");
    }
    printf("[TEST M14] OK: Allocated PTY pair (master fd %d, slave fd %d on '%s')\n",
           master_fd, slave_fd, pty_name);

    if (strncmp(pty_name, "/dev/pts/", 9) != 0) {
        check_fail("Invalid slave PTY device path");
    }

    /* Master writes to slave */
    const char *master_msg = "Hello AuroraOS PTY Line Discipline!\n";
    ssize_t written = write(master_fd, master_msg, strlen(master_msg));
    if (written != (ssize_t)strlen(master_msg)) {
        check_fail("write to pty master");
    }

    char slave_rx[128];
    ssize_t slave_n = read(slave_fd, slave_rx, sizeof(slave_rx) - 1);
    if (slave_n <= 0) {
        check_fail("read from pty slave");
    }
    slave_rx[slave_n] = '\0';
    if (strcmp(slave_rx, master_msg) != 0) {
        check_fail("PTY canonical line content mismatch");
    }
    printf("[TEST M14] OK: Master -> Slave input discipline verified: '%s'", slave_rx);

    /* Slave writes to master */
    const char *slave_msg = "Slave terminal response\n";
    written = write(slave_fd, slave_msg, strlen(slave_msg));
    if (written != (ssize_t)strlen(slave_msg)) {
        check_fail("write to pty slave");
    }

    char master_rx[128];
    ssize_t master_n = read(master_fd, master_rx, sizeof(master_rx) - 1);
    if (master_n <= 0) {
        check_fail("read from pty master");
    }
    master_rx[master_n] = '\0';
    if (!strstr(master_rx, "Slave terminal response")) {
        check_fail("PTY slave -> master output mismatch");
    }
    printf("[TEST M14] OK: Slave -> Master output discipline verified: '%s'", master_rx);

    close(slave_fd);
    close(master_fd);
    printf("[TEST M14] OK: PTY subsystem teardown complete\n");

    /* Subtest 2: DHCP Client & /etc/resolv.conf */
    printf("\n--- Subtest 2: DHCP Client & /etc/resolv.conf Verification ---\n");
    int rfd = open("/etc/resolv.conf", O_RDONLY, 0);
    if (rfd < 0) {
        check_fail("open /etc/resolv.conf");
    }
    char rconf[256];
    int rn = read(rfd, rconf, sizeof(rconf) - 1);
    close(rfd);
    if (rn <= 0) check_fail("read /etc/resolv.conf");
    rconf[rn] = '\0';
    if (!strstr(rconf, "nameserver 10.0.2.3")) {
        check_fail("nameserver 10.0.2.3 not found in /etc/resolv.conf");
    }
    printf("[TEST M14] OK: /etc/resolv.conf content verified:\n%s", rconf);

    /* Run /bin/dhclient */
    pid_t dpid = fork();
    if (dpid < 0) check_fail("fork dhclient");
    if (dpid == 0) {
        char *argv[] = { "/bin/dhclient", "eth0", NULL };
        execve("/bin/dhclient", argv, NULL);
        exit(1);
    }
    int dstatus = 0;
    wait4(dpid, &dstatus, 0, NULL);
    if (dstatus != 0) check_fail("dhclient exited with error");
    printf("[TEST M14] OK: /bin/dhclient executed successfully\n");

    /* Subtest 3: DNS Resolver & nslookup */
    printf("\n--- Subtest 3: DNS Resolver & /bin/nslookup Verification ---\n");
    struct hostent *he = gethostbyname("localhost");
    if (!he || !he->h_addr_list[0]) {
        check_fail("gethostbyname('localhost')");
    }
    struct in_addr local_in;
    memcpy(&local_in.s_addr, he->h_addr_list[0], 4);
    if (strcmp(inet_ntoa(local_in), "127.0.0.1") != 0) {
        check_fail("localhost resolved IP mismatch");
    }
    printf("[TEST M14] OK: gethostbyname('localhost') resolved to %s\n", inet_ntoa(local_in));

    he = gethostbyname("aurora");
    if (!he || !he->h_addr_list[0]) {
        check_fail("gethostbyname('aurora')");
    }
    memcpy(&local_in.s_addr, he->h_addr_list[0], 4);
    printf("[TEST M14] OK: gethostbyname('aurora') resolved to %s\n", inet_ntoa(local_in));

    /* Run /bin/nslookup */
    pid_t nspid = fork();
    if (nspid < 0) check_fail("fork nslookup");
    if (nspid == 0) {
        char *argv[] = { "/bin/nslookup", "localhost", NULL };
        execve("/bin/nslookup", argv, NULL);
        exit(1);
    }
    int nsstatus = 0;
    wait4(nspid, &nsstatus, 0, NULL);
    if (nsstatus != 0) check_fail("nslookup exited with error");
    printf("[TEST M14] OK: /bin/nslookup utility verified\n");

    /* Subtest 4: Network Diagnostic Utilities (ifconfig, netstat) */
    printf("\n--- Subtest 4: /bin/ifconfig and /bin/netstat Verification ---\n");
    pid_t ifpid = fork();
    if (ifpid < 0) check_fail("fork ifconfig");
    if (ifpid == 0) {
        char *argv[] = { "/bin/ifconfig", NULL };
        execve("/bin/ifconfig", argv, NULL);
        exit(1);
    }
    int ifstatus = 0;
    wait4(ifpid, &ifstatus, 0, NULL);
    if (ifstatus != 0) check_fail("ifconfig exited with error");
    printf("[TEST M14] OK: /bin/ifconfig executed successfully\n");

    pid_t nstpid = fork();
    if (nstpid < 0) check_fail("fork netstat");
    if (nstpid == 0) {
        char *argv[] = { "/bin/netstat", "-r", NULL };
        execve("/bin/netstat", argv, NULL);
        exit(1);
    }
    int nststatus = 0;
    wait4(nstpid, &nststatus, 0, NULL);
    if (nststatus != 0) check_fail("netstat -r exited with error");
    printf("[TEST M14] OK: /bin/netstat -r executed successfully\n");

    /* Subtest 5: HTTP Application Client (wget) Verification */
    printf("\n--- Subtest 5: /bin/wget HTTP Download Verification ---\n");
    int srv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_sock < 0) check_fail("socket for HTTP server");

    struct sockaddr_in srv_sin;
    memset(&srv_sin, 0, sizeof(srv_sin));
    srv_sin.sin_family = AF_INET;
    srv_sin.sin_port = htons(8080);
    srv_sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(srv_sock, (struct sockaddr *)&srv_sin, sizeof(srv_sin)) < 0) {
        check_fail("bind HTTP server 8080");
    }
    if (listen(srv_sock, 2) < 0) {
        check_fail("listen HTTP server");
    }

    const char *payload_text = "Milestone M14 HTTP Payload Downloaded 100%!";

    pid_t srv_pid = fork();
    if (srv_pid < 0) check_fail("fork HTTP server child");
    if (srv_pid == 0) {
        /* HTTP server child */
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int cfd = accept(srv_sock, (struct sockaddr *)&caddr, &clen);
        if (cfd >= 0) {
            char req[512];
            read(cfd, req, sizeof(req) - 1);

            char resp[512];
            snprintf(resp, sizeof(resp),
                     "HTTP/1.0 200 OK\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n\r\n%s",
                     (int)strlen(payload_text), payload_text);
            write(cfd, resp, strlen(resp));
            close(cfd);
        }
        close(srv_sock);
        exit(0);
    }

    /* Parent runs /bin/wget to download file from HTTP server */
    pid_t wget_pid = fork();
    if (wget_pid < 0) check_fail("fork wget");
    if (wget_pid == 0) {
        char *argv[] = {
            "/bin/wget",
            "http://127.0.0.1:8080/test.txt",
            "-O",
            "/tmp/m14_download.txt",
            NULL
        };
        execve("/bin/wget", argv, NULL);
        exit(1);
    }

    int wget_status = 0;
    wait4(wget_pid, &wget_status, 0, NULL);
    if (wget_status != 0) {
        check_fail("wget exited with failure status");
    }

    int srv_status = 0;
    wait4(srv_pid, &srv_status, 0, NULL);
    close(srv_sock);

    /* Verify downloaded payload byte for byte */
    int dfd = open("/tmp/m14_download.txt", O_RDONLY, 0);
    if (dfd < 0) check_fail("open /tmp/m14_download.txt");
    char dl_buf[128];
    int dn = read(dfd, dl_buf, sizeof(dl_buf) - 1);
    close(dfd);
    if (dn <= 0) check_fail("read /tmp/m14_download.txt");
    dl_buf[dn] = '\0';

    if (strcmp(dl_buf, payload_text) != 0) {
        printf("Expected: '%s'\nGot:      '%s'\n", payload_text, dl_buf);
        check_fail("Downloaded file payload mismatch");
    }
    printf("[TEST M14] OK: Downloaded payload matches server text byte-for-byte:\n  '%s'\n", dl_buf);

    printf("\n============================================================\n");
    printf("Milestone M14 status: [OK]\n");
    printf(">>> TEST PASSED: Milestone M14 Acceptance Criteria Verified! <<<\n");
    printf("============================================================\n");

    return 0;
}
