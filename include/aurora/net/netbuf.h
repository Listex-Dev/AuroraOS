#ifndef _AURORA_NET_NETBUF_H
#define _AURORA_NET_NETBUF_H

#include <aurora/types.h>

#define NETBUF_MAX_SIZE 2048
#define NETBUF_HEADROOM 128

struct netdev;

typedef struct netbuf {
    uint8_t *head;
    uint8_t *data;
    uint8_t *tail;
    uint8_t *end;
    uint32_t len;
    struct netdev *dev;
    struct netbuf *next;
    uint8_t buf[NETBUF_MAX_SIZE];
} netbuf_t;

netbuf_t *netbuf_alloc(size_t size);
void netbuf_free(netbuf_t *nb);
uint8_t *netbuf_put(netbuf_t *nb, size_t len);
uint8_t *netbuf_push(netbuf_t *nb, size_t len);
uint8_t *netbuf_pull(netbuf_t *nb, size_t len);
void netbuf_reserve(netbuf_t *nb, size_t len);

#endif /* _AURORA_NET_NETBUF_H */
