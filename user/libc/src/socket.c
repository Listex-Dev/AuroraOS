#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "syscall.h"

int socket(int domain, int type, int protocol)
{
    return (int)__syscall_ret(__syscall3(SYS_socket, domain, type, protocol));
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return (int)__syscall_ret(__syscall3(SYS_bind, sockfd, (long)addr, addrlen));
}

int listen(int sockfd, int backlog)
{
    return (int)__syscall_ret(__syscall2(SYS_listen, sockfd, backlog));
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return (int)__syscall_ret(__syscall3(SYS_accept, sockfd, (long)addr, (long)addrlen));
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return (int)__syscall_ret(__syscall3(SYS_connect, sockfd, (long)addr, addrlen));
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    return (ssize_t)__syscall_ret(__syscall6(SYS_sendto, sockfd, (long)buf, len, flags, 0, 0));
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    return (ssize_t)__syscall_ret(__syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, 0, 0));
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
    return (ssize_t)__syscall_ret(__syscall6(SYS_sendto, sockfd, (long)buf, len, flags, (long)dest_addr, addrlen));
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen)
{
    return (ssize_t)__syscall_ret(__syscall6(SYS_recvfrom, sockfd, (long)buf, len, flags, (long)src_addr, (long)addrlen));
}

int shutdown(int sockfd, int how)
{
    return (int)__syscall_ret(__syscall2(SYS_shutdown, sockfd, how));
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return (int)__syscall_ret(__syscall3(SYS_getsockname, sockfd, (long)addr, (long)addrlen));
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return (int)__syscall_ret(__syscall3(SYS_getpeername, sockfd, (long)addr, (long)addrlen));
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    return (int)__syscall_ret(__syscall5(SYS_setsockopt, sockfd, level, optname, (long)optval, optlen));
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    return (int)__syscall_ret(__syscall5(SYS_getsockopt, sockfd, level, optname, (long)optval, (long)optlen));
}

/* Byte order helpers */
uint16_t htons(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

uint16_t ntohs(uint16_t v)
{
    return htons(v);
}

uint32_t htonl(uint32_t v)
{
    return ((v >> 24) & 0xff) |
           ((v >> 8)  & 0xff00) |
           ((v << 8)  & 0xff0000) |
           ((v << 24) & 0xff000000);
}

uint32_t ntohl(uint32_t v)
{
    return htonl(v);
}

/* IP address formatting */
in_addr_t inet_addr(const char *cp)
{
    if (!cp) return 0xFFFFFFFF;
    unsigned int val = 0;
    unsigned int parts[4] = {0};
    int part = 0;

    for (const char *p = cp; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            if (val > 255) return 0xFFFFFFFF;
        } else if (*p == '.') {
            if (part >= 3) return 0xFFFFFFFF;
            parts[part++] = val;
            val = 0;
        } else {
            return 0xFFFFFFFF;
        }
    }
    if (part != 3) return 0xFFFFFFFF;
    parts[3] = val;

    uint32_t ip = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return htonl(ip);
}

char *inet_ntoa(struct in_addr in)
{
    static char buf[32];
    uint32_t ip = ntohl(in.s_addr);
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
             (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
    return buf;
}

int inet_pton(int af, const char *src, void *dst)
{
    if (af != AF_INET || !src || !dst) {
        return -1;
    }
    in_addr_t addr = inet_addr(src);
    if (addr == 0xFFFFFFFF && strcmp(src, "255.255.255.255") != 0) {
        return 0;
    }
    *(in_addr_t *)dst = addr;
    return 1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (af != AF_INET || !src || !dst || size < 16) {
        return NULL;
    }
    struct in_addr in;
    in.s_addr = *(const in_addr_t *)src;
    char *res = inet_ntoa(in);
    strncpy(dst, res, size - 1);
    dst[size - 1] = '\0';
    return dst;
}
