#ifndef _AURORA_NET_SOCKET_H
#define _AURORA_NET_SOCKET_H

#include <aurora/types.h>
#include <aurora/fs/vfs.h>
#include <aurora/net/tcp.h>

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_LOCAL  1
#define AF_INET   2
#define AF_INET6  10

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define SOL_SOCKET 1

#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10
#define SO_NO_CHECK     11
#define SO_PRIORITY     12
#define SO_LINGER       13
#define SO_BSDCOMPAT    14
#define SO_REUSEPORT    15

#define TCP_NODELAY     1

typedef uint32_t socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    uint16_t       sin_family;
    in_port_t      sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

#define INADDR_ANY       ((in_addr_t)0x00000000)
#define INADDR_LOOPBACK  ((in_addr_t)0x7f000001)

#define SOCKET_BUFFER_SIZE 65536

struct socket {
    int family;
    int type;
    int protocol;
    int state;

    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    /* TCP specific state */
    uint32_t snd_una;
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
    uint32_t rcv_wnd;

    /* Receive buffer queue */
    uint8_t *rx_buf;
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t rx_count;

    /* Accept queue for listening stream sockets */
    struct socket *accept_queue[16];
    int accept_head;
    int accept_tail;
    int accept_count;
    int backlog;

    /* Socket options */
    int so_reuseaddr;
    int so_keepalive;
    int so_error;

    int ref_count;
    struct file *file;
    struct socket *next;
};

extern struct file_operations g_socket_fops;

void socket_subsys_init(void);
struct socket *socket_create(int family, int type, int protocol);
void socket_destroy(struct socket *sock);
int socket_bind(struct socket *sock, const struct sockaddr *addr, socklen_t addrlen);
int socket_listen(struct socket *sock, int backlog);
int socket_accept(struct socket *sock, struct sockaddr *addr, socklen_t *addrlen, struct socket **new_sock);
int socket_connect(struct socket *sock, const struct sockaddr *addr, socklen_t addrlen);
ssize_t socket_send(struct socket *sock, const void *buf, size_t len, int flags);
ssize_t socket_recv(struct socket *sock, void *buf, size_t len, int flags);
ssize_t socket_sendto(struct socket *sock, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t socket_recvfrom(struct socket *sock, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen);
int socket_shutdown(struct socket *sock, int how);
int socket_enqueue_rx(struct socket *sock, const void *data, size_t len);

/* System Call handlers */
int64_t sys_socket(int domain, int type, int protocol);
int64_t sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int64_t sys_listen(int sockfd, int backlog);
int64_t sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int64_t sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int64_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen);
int64_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen);
int64_t sys_shutdown(int sockfd, int how);
int64_t sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int64_t sys_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int64_t sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int64_t sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

#endif /* _AURORA_NET_SOCKET_H */
