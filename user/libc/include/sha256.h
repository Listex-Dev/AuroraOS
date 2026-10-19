#ifndef _SHA256_H
#define _SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t digest[32]);
void sha256_hash_bytes(const void *data, size_t len, char hex_out[65]);
int  sha256_hash_file(const char *path, char hex_out[65]);
void sha256_hmac(const void *key, size_t key_len, const void *data, size_t data_len, char hex_out[65]);

#endif /* _SHA256_H */
