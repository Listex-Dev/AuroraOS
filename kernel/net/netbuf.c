#include <aurora/net/netbuf.h>
#include <aurora/mm/heap.h>
#include <aurora/string.h>

netbuf_t *netbuf_alloc(size_t size)
{
    (void)size;
    netbuf_t *nb = (netbuf_t *)kmalloc(sizeof(netbuf_t), GFP_KERNEL);
    if (!nb) {
        return NULL;
    }

    memset(nb, 0, sizeof(netbuf_t));
    nb->head = nb->buf;
    nb->end = nb->buf + NETBUF_MAX_SIZE;
    nb->data = nb->head + NETBUF_HEADROOM;
    nb->tail = nb->data;
    nb->len = 0;
    nb->dev = NULL;
    nb->next = NULL;

    return nb;
}

void netbuf_free(netbuf_t *nb)
{
    if (nb) {
        kfree(nb);
    }
}

uint8_t *netbuf_put(netbuf_t *nb, size_t len)
{
    if (!nb || (nb->tail + len > nb->end)) {
        return NULL;
    }
    uint8_t *tmp = nb->tail;
    nb->tail += len;
    nb->len += (uint32_t)len;
    return tmp;
}

uint8_t *netbuf_push(netbuf_t *nb, size_t len)
{
    if (!nb || (nb->data - len < nb->head)) {
        return NULL;
    }
    nb->data -= len;
    nb->len += (uint32_t)len;
    return nb->data;
}

uint8_t *netbuf_pull(netbuf_t *nb, size_t len)
{
    if (!nb || (len > nb->len)) {
        return NULL;
    }
    uint8_t *tmp = nb->data;
    nb->data += len;
    nb->len -= (uint32_t)len;
    return tmp;
}

void netbuf_reserve(netbuf_t *nb, size_t len)
{
    if (!nb) {
        return;
    }
    nb->data += len;
    nb->tail += len;
}
