/*
 * AuroraOS libc: SHA-256 & HMAC-SHA256 (FIPS 180-2, RFC 2104)
 */

#include <sha256.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define Ch(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x) (ROR32(x, 2) ^ ROR32(x, 13) ^ ROR32(x, 22))
#define Sigma1(x) (ROR32(x, 6) ^ ROR32(x, 11) ^ ROR32(x, 25))
#define sigma0(x) (ROR32(x, 7) ^ ROR32(x, 18) ^ ((x) >> 3))
#define sigma1(x) (ROR32(x, 17) ^ ROR32(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void sha256_init(sha256_ctx *ctx)
{
    if (!ctx) return;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_transform(uint32_t state[8], const uint8_t data[64])
{
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)data[i * 4 + 0] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8)  |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
        uint32_t T2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256_update(sha256_ctx *ctx, const void *data, size_t len)
{
    if (!ctx || !data || len == 0) return;

    const uint8_t *p = (const uint8_t *)data;
    size_t index = (size_t)(ctx->count & 0x3F);
    ctx->count += len;

    size_t part_len = 64 - index;
    size_t i = 0;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], p, part_len);
        sha256_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx->state, &p[i]);
        }
        index = 0;
    }

    memcpy(&ctx->buffer[index], &p[i], len - i);
}

void sha256_final(sha256_ctx *ctx, uint8_t digest[32])
{
    if (!ctx || !digest) return;

    uint8_t final_count[8];
    uint64_t total_bits = ctx->count * 8;
    for (int i = 0; i < 8; i++) {
        final_count[i] = (uint8_t)((total_bits >> ((7 - i) * 8)) & 0xFF);
    }

    size_t index = (size_t)(ctx->count & 0x3F);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    static const uint8_t padding[64] = { 0x80 };
    sha256_update(ctx, padding, pad_len);
    sha256_update(ctx, final_count, 8);

    for (int i = 0; i < 8; i++) {
        digest[i * 4 + 0] = (uint8_t)((ctx->state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8)  & 0xFF);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xFF);
    }
}

static void bytes_to_hex(const uint8_t in[32], char hex_out[65])
{
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex_out[i * 2 + 0] = hex_chars[(in[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[in[i] & 0x0F];
    }
    hex_out[64] = '\0';
}

void sha256_hash_bytes(const void *data, size_t len, char hex_out[65])
{
    sha256_ctx ctx;
    uint8_t digest[32];
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
    bytes_to_hex(digest, hex_out);
}

int sha256_hash_file(const char *path, char hex_out[65])
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    sha256_ctx ctx;
    sha256_init(&ctx);

    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        sha256_update(&ctx, buf, (size_t)n);
    }
    close(fd);

    if (n < 0) return -1;

    uint8_t digest[32];
    sha256_final(&ctx, digest);
    bytes_to_hex(digest, hex_out);
    return 0;
}

void sha256_hmac(const void *key, size_t key_len, const void *data, size_t data_len, char hex_out[65])
{
    uint8_t k_block[64];
    memset(k_block, 0, sizeof(k_block));

    if (key_len > 64) {
        sha256_ctx k_ctx;
        sha256_init(&k_ctx);
        sha256_update(&k_ctx, key, key_len);
        sha256_final(&k_ctx, k_block);
    } else {
        memcpy(k_block, key, key_len);
    }

    uint8_t k_ipad[64];
    uint8_t k_opad[64];
    for (int i = 0; i < 64; i++) {
        k_ipad[i] = k_block[i] ^ 0x36;
        k_opad[i] = k_block[i] ^ 0x5C;
    }

    /* Inner hash */
    sha256_ctx in_ctx;
    uint8_t in_digest[32];
    sha256_init(&in_ctx);
    sha256_update(&in_ctx, k_ipad, 64);
    sha256_update(&in_ctx, data, data_len);
    sha256_final(&in_ctx, in_digest);

    /* Outer hash */
    sha256_ctx out_ctx;
    uint8_t out_digest[32];
    sha256_init(&out_ctx);
    sha256_update(&out_ctx, k_opad, 64);
    sha256_update(&out_ctx, in_digest, 32);
    sha256_final(&out_ctx, out_digest);

    bytes_to_hex(out_digest, hex_out);
}
