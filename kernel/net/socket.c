#include <aurora/net/socket.h>
#include <aurora/net/udp.h>
#include <aurora/net/tcp.h>
#include <aurora/net/ipv4.h>
#include <aurora/net/icmp.h>
#include <aurora/net/netdev.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/heap.h>
#include <aurora/errno.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static ssize_t socket_vfs_read(struct file *file, void *buf, size_t count, uint64_t *pos);
static ssize_t socket_vfs_write(struct file *file, const void *buf, size_t count, uint64_t *pos);
static int socket_vfs_release(struct inode *inode, struct file *file);
static int socket_vfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

struct file_operations g_socket_fops = {
    .open    = NULL,
    .release = socket_vfs_release,
    .read    = socket_vfs_read,
    .write   = socket_vfs_write,
    .llseek  = NULL,
    .ioctl   = socket_vfs_ioctl
};

void socket_subsys_init(void)
{
    printk("<6>[  0.000000] Net: BSD Socket layer initialized\n");
}

struct socket *socket_create(int family, int type, int protocol)
{
    struct socket *sock = (struct socket *)kzalloc(sizeof(struct socket), GFP_KERNEL);
    if (!sock) {
        return NULL;
    }

    sock->family = family;
    sock->type = type;
    sock->protocol = protocol;
    sock->state = 0;
    sock->ref_count = 1;

    sock->rx_buf = (uint8_t *)kzalloc(SOCKET_BUFFER_SIZE, GFP_KERNEL);
    if (!sock->rx_buf) {
        kfree(sock);
        return NULL;
    }

    sock->rx_head = 0;
    sock->rx_tail = 0;
    sock->rx_count = 0;

    return sock;
}

void socket_destroy(struct socket *sock)
{
    if (!sock) {
        return;
    }

    sock->ref_count--;
    if (sock->ref_count <= 0) {
        if (sock->type == SOCK_DGRAM) {
            udp_unbind(sock);
        } else if (sock->type == SOCK_STREAM) {
            tcp_close(sock);
        }

        if (sock->rx_buf) {
            kfree(sock->rx_buf);
        }
        kfree(sock);
    }
}

int socket_enqueue_rx(struct socket *sock, const void *data, size_t len)
{
    if (!sock || !data || len == 0 || !sock->rx_buf) {
        return -1;
    }

    const uint8_t *src = (const uint8_t *)data;
    size_t copied = 0;

    while (copied < len && sock->rx_count < SOCKET_BUFFER_SIZE) {
        sock->rx_buf[sock->rx_head] = src[copied++];
        sock->rx_head = (sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
        sock->rx_count++;
    }

    return (int)copied;
}

int socket_dequeue_rx(struct socket *sock, void *buf, size_t len)
{
    if (!sock || !buf || len == 0 || !sock->rx_buf || sock->rx_count == 0) {
        return 0;
    }

    uint8_t *dst = (uint8_t *)buf;
    size_t copied = 0;

    while (copied < len && sock->rx_count > 0) {
        dst[copied++] = sock->rx_buf[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        sock->rx_count--;
    }

    return (int)copied;
}

static ssize_t socket_vfs_read(struct file *file, void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    if (!file || !file->private_data || !buf || count == 0) {
        return 0;
    }

    struct socket *sock = (struct socket *)file->private_data;
    struct netdev *dev = netdev_get_default();

    if (sock->type == SOCK_STREAM) {
        while (sock->rx_count == 0 && (sock->state == TCP_ESTABLISHED || sock->state == TCP_SYN_RECEIVED)) {
            if (file->f_flags & O_NONBLOCK) {
                return -EAGAIN;
            }
            if (dev && dev->ops && dev->ops->poll) {
                dev->ops->poll(dev);
            }
            sched_yield();
        }
        if (sock->rx_count == 0) {
            return 0; /* EOF */
        }
    } else {
        for (int iter = 0; iter < 100 && sock->rx_count == 0; iter++) {
            if (dev && dev->ops && dev->ops->poll) {
                dev->ops->poll(dev);
            }
            if (sock->rx_count > 0) break;
            sched_yield();
        }
        if (sock->rx_count == 0) {
            if (file->f_flags & O_NONBLOCK) return -EAGAIN;
            return 0;
        }
    }

    uint8_t *dst = (uint8_t *)buf;
    size_t read_bytes = 0;

    while (read_bytes < count && sock->rx_count > 0) {
        dst[read_bytes++] = sock->rx_buf[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        sock->rx_count--;
    }

    return (ssize_t)read_bytes;
}

static ssize_t socket_vfs_write(struct file *file, const void *buf, size_t count, uint64_t *pos)
{
    (void)pos;
    if (!file || !file->private_data || !buf || count == 0) {
        return 0;
    }

    struct socket *sock = (struct socket *)file->private_data;
    return socket_send(sock, buf, count, 0);
}

static int socket_vfs_release(struct inode *inode, struct file *file)
{
    (void)inode;
    if (file && file->private_data) {
        socket_destroy((struct socket *)file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

#define FIONREAD 0x541B

static int socket_vfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (!file || !file->private_data) {
        return -EBADF;
    }

    struct socket *sock = (struct socket *)file->private_data;
    if (cmd == FIONREAD) {
        if (!arg) {
            return -EFAULT;
        }
        struct netdev *dev = netdev_get_default();
        if (dev && dev->ops && dev->ops->poll) {
            dev->ops->poll(dev);
        }
        *(int *)arg = (int)sock->rx_count;
        return 0;
    }

    return -EINVAL;
}

ssize_t socket_send(struct socket *sock, const void *buf, size_t len, int flags)
{
    (void)flags;
    if (!sock || !buf) {
        return -EINVAL;
    }

    if (sock->type == SOCK_DGRAM) {
        int ret = udp_send(sock, sock->remote_ip, sock->remote_port, buf, len);
        return ret == 0 ? (ssize_t)len : ret;
    } else if (sock->type == SOCK_STREAM) {
        return tcp_send(sock, buf, len);
    } else if (sock->type == SOCK_RAW) {
        int ret = icmp_send_echo(sock->remote_ip, 0x4321, 1, buf, len);
        return ret == 0 ? (ssize_t)len : ret;
    }

    return -EPROTONOSUPPORT;
}

ssize_t socket_recv(struct socket *sock, void *buf, size_t len, int flags)
{
    (void)flags;
    if (!sock || !buf) {
        return -EINVAL;
    }

    if (sock->type == SOCK_RAW) {
        int ret = icmp_wait_reply(0x4321, 1, 1000);
        if (ret == 0) {
            size_t ret_len = len < 64 ? len : 64;
            memset(buf, 0, ret_len);
            return (ssize_t)ret_len;
        }
        return 0;
    }

    struct netdev *dev = netdev_get_default();
    if (sock->type == SOCK_STREAM) {
        while (sock->rx_count == 0 && sock->state == TCP_ESTABLISHED) {
            if (dev && dev->ops && dev->ops->poll) {
                dev->ops->poll(dev);
            }
            sched_yield();
        }
        if (sock->rx_count == 0) {
            return 0; /* EOF */
        }
    } else {
        for (int iter = 0; iter < 100 && sock->rx_count == 0; iter++) {
            if (dev && dev->ops && dev->ops->poll) {
                dev->ops->poll(dev);
            }
            if (sock->rx_count > 0) break;
            sched_yield();
        }
        if (sock->rx_count == 0) {
            return 0;
        }
    }

    uint8_t *dst = (uint8_t *)buf;
    size_t read_bytes = 0;
    while (read_bytes < len && sock->rx_count > 0) {
        dst[read_bytes++] = sock->rx_buf[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        sock->rx_count--;
    }

    return (ssize_t)read_bytes;
}

/* System Calls */

static struct files_struct *get_current_files(void)
{
    task_t *curr = sched_current();
    if (!curr) {
        return NULL;
    }
    if (!curr->files) {
        curr->files = files_alloc();
    }
    return (struct files_struct *)curr->files;
}

static struct socket *get_socket_from_fd(int fd)
{
    if (fd < 0 || fd >= NR_OPEN) {
        return NULL;
    }
    struct files_struct *files = get_current_files();
    if (!files || !files->fd[fd]) {
        return NULL;
    }
    if (files->fd[fd]->f_op != &g_socket_fops) {
        return NULL;
    }
    return (struct socket *)files->fd[fd]->private_data;
}

int64_t sys_socket(int domain, int type, int protocol)
{
    if (domain != AF_INET && domain != AF_UNIX) {
        return -EAFNOSUPPORT;
    }

    if (protocol == 0) {
        if (type == SOCK_STREAM) protocol = IPPROTO_TCP;
        else if (type == SOCK_DGRAM) protocol = IPPROTO_UDP;
    }

    struct socket *sock = socket_create(domain, type, protocol);
    if (!sock) {
        return -ENOMEM;
    }

    struct file *file = (struct file *)kzalloc(sizeof(struct file), GFP_KERNEL);
    if (!file) {
        socket_destroy(sock);
        return -ENOMEM;
    }

    file->f_op = &g_socket_fops;
    file->f_flags = O_RDWR;
    file->f_mode = S_IFSOCK | 0666;
    file->f_count = 1;
    file->private_data = sock;
    sock->file = file;

    struct files_struct *files = get_current_files();
    int free_fd = -1;
    for (int i = 0; i < NR_OPEN; i++) {
        if (!files->fd[i]) {
            free_fd = i;
            break;
        }
    }

    if (free_fd < 0) {
        kfree(file);
        socket_destroy(sock);
        return -EMFILE;
    }

    files->fd[free_fd] = file;
    return free_fd;
}

int64_t sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    (void)addrlen;
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!addr) {
        return -EFAULT;
    }
    if (addr->sa_family != AF_INET) {
        return -EAFNOSUPPORT;
    }

    const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
    uint16_t port = net_ntohs(sin->sin_port);
    sock->local_ip = sin->sin_addr.s_addr;

    if (sock->type == SOCK_DGRAM) {
        return udp_bind(sock, port);
    } else if (sock->type == SOCK_STREAM) {
        return tcp_bind(sock, port);
    }

    return -EPROTONOSUPPORT;
}

int64_t sys_listen(int sockfd, int backlog)
{
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }

    if (sock->type == SOCK_STREAM) {
        return tcp_listen(sock, backlog);
    }

    return -EOPNOTSUPP;
}

int64_t sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }

    if (sock->type != SOCK_STREAM) {
        return -EOPNOTSUPP;
    }
    if (sock->state != TCP_LISTEN) {
        return -EINVAL;
    }

    struct socket *child = NULL;
    struct netdev *dev = netdev_get_default();

    while (!(child = tcp_accept(sock))) {
        if (sock->file && (sock->file->f_flags & O_NONBLOCK)) {
            return -EAGAIN;
        }
        if (dev && dev->ops && dev->ops->poll) {
            dev->ops->poll(dev);
        }
        sched_yield();
    }

    struct file *file = (struct file *)kzalloc(sizeof(struct file), GFP_KERNEL);
    if (!file) {
        socket_destroy(child);
        return -ENOMEM;
    }

    file->f_op = &g_socket_fops;
    file->f_flags = O_RDWR;
    file->f_mode = S_IFSOCK | 0666;
    file->f_count = 1;
    file->private_data = child;
    child->file = file;

    struct files_struct *files = get_current_files();
    int free_fd = -1;
    for (int i = 0; i < NR_OPEN; i++) {
        if (!files->fd[i]) {
            free_fd = i;
            break;
        }
    }

    if (free_fd < 0) {
        kfree(file);
        socket_destroy(child);
        return -EMFILE;
    }

    files->fd[free_fd] = file;

    if (addr && addrlen) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        sin->sin_family = AF_INET;
        sin->sin_port = net_htons(child->remote_port);
        sin->sin_addr.s_addr = child->remote_ip;
        *addrlen = sizeof(struct sockaddr_in);
    }

    return free_fd;
}

int64_t sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    (void)addrlen;
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!addr) {
        return -EFAULT;
    }
    if (addr->sa_family != AF_INET) {
        return -EAFNOSUPPORT;
    }

    const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
    uint32_t dst_ip = sin->sin_addr.s_addr;
    uint16_t dst_port = net_ntohs(sin->sin_port);

    if (sock->type == SOCK_DGRAM) {
        sock->remote_ip = dst_ip;
        sock->remote_port = dst_port;
        return 0;
    } else if (sock->type == SOCK_STREAM) {
        return tcp_connect(sock, dst_ip, dst_port);
    }

    return -EPROTONOSUPPORT;
}

int64_t sys_sendto(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen)
{
    (void)flags; (void)addrlen;
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!buf) {
        return -EFAULT;
    }

    if (dest_addr) {
        if (dest_addr->sa_family != AF_INET) {
            return -EAFNOSUPPORT;
        }
        const struct sockaddr_in *sin = (const struct sockaddr_in *)dest_addr;
        uint32_t dst_ip = sin->sin_addr.s_addr;
        uint16_t dst_port = net_ntohs(sin->sin_port);

        if (sock->type == SOCK_DGRAM) {
            int ret = udp_send(sock, dst_ip, dst_port, buf, len);
            return ret == 0 ? (int64_t)len : ret;
        } else if (sock->type == SOCK_RAW) {
            int ret = icmp_send_echo(dst_ip, 0x4321, 1, buf, len);
            return ret == 0 ? (int64_t)len : ret;
        }
    }

    return socket_send(sock, buf, len, flags);
}

int64_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen)
{
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!buf) {
        return -EFAULT;
    }

    ssize_t ret = socket_recv(sock, buf, len, flags);

    if (ret > 0 && src_addr && addrlen) {
        struct sockaddr_in *sin = (struct sockaddr_in *)src_addr;
        sin->sin_family = AF_INET;
        sin->sin_port = net_htons(sock->remote_port);
        sin->sin_addr.s_addr = sock->remote_ip;
        *addrlen = sizeof(struct sockaddr_in);
    }

    return ret;
}

int64_t sys_shutdown(int sockfd, int how)
{
    (void)how;
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }

    if (sock->type == SOCK_STREAM) {
        tcp_close(sock);
    }
    return 0;
}

int64_t sys_getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!addr || !addrlen) {
        return -EFAULT;
    }

    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    sin->sin_family = AF_INET;
    sin->sin_port = net_htons(sock->local_port);
    sin->sin_addr.s_addr = sock->local_ip;
    *addrlen = sizeof(struct sockaddr_in);
    return 0;
}

int64_t sys_getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!addr || !addrlen) {
        return -EFAULT;
    }

    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    sin->sin_family = AF_INET;
    sin->sin_port = net_htons(sock->remote_port);
    sin->sin_addr.s_addr = sock->remote_ip;
    *addrlen = sizeof(struct sockaddr_in);
    return 0;
}

int64_t sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    (void)level; (void)optval; (void)optlen;
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }

    if (optname == SO_REUSEADDR) {
        sock->so_reuseaddr = 1;
    } else if (optname == SO_KEEPALIVE) {
        sock->so_keepalive = 1;
    }

    return 0;
}

int64_t sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    (void)level; (void)optname;
    struct socket *sock = get_socket_from_fd(sockfd);
    if (!sock) {
        return -EBADF;
    }
    if (!optval || !optlen) {
        return -EFAULT;
    }

    if (*optlen >= sizeof(int)) {
        *(int *)optval = 0;
        *optlen = sizeof(int);
    }
    return 0;
}
