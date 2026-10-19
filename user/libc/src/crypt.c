/*
 * SHA-512 Unix Crypt implementation (RFC / Ulrich Drepper specification)
 */

#include <crypt.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    uint64_t state[8];
    uint64_t count[2];
    uint8_t buffer[128];
} sha512_ctx;

#define ROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define Ch(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x) (ROR64(x, 28) ^ ROR64(x, 34) ^ ROR64(x, 39))
#define Sigma1(x) (ROR64(x, 14) ^ ROR64(x, 18) ^ ROR64(x, 41))
#define sigma0(x) (ROR64(x, 1) ^ ROR64(x, 8) ^ ((x) >> 7))
#define sigma1(x) (ROR64(x, 19) ^ ROR64(x, 61) ^ ((x) >> 6))

static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7bee0ee36ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0x9c30d532ef8444b1ULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL,
    0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL,
    0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
    0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL,
    0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL,
    0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL,
    0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL
};

static void sha512_init(sha512_ctx *ctx)
{
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

static void sha512_transform(uint64_t state[8], const uint8_t data[128])
{
    uint64_t W[80];
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint64_t)data[i * 8 + 0] << 56) |
               ((uint64_t)data[i * 8 + 1] << 48) |
               ((uint64_t)data[i * 8 + 2] << 40) |
               ((uint64_t)data[i * 8 + 3] << 32) |
               ((uint64_t)data[i * 8 + 4] << 24) |
               ((uint64_t)data[i * 8 + 5] << 16) |
               ((uint64_t)data[i * 8 + 6] << 8)  |
               ((uint64_t)data[i * 8 + 7]);
    }
    for (int i = 16; i < 80; i++) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }

    uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint64_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 80; i++) {
        uint64_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
        uint64_t T2 = Sigma0(a) + Maj(a, b, c);
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

static void sha512_update(sha512_ctx *ctx, const void *in_data, size_t len)
{
    const uint8_t *data = (const uint8_t *)in_data;
    size_t idx = (size_t)((ctx->count[0] >> 3) & 127);
    ctx->count[0] += ((uint64_t)len << 3);
    if (ctx->count[0] < ((uint64_t)len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += ((uint64_t)len >> 61);

    size_t part_len = 128 - idx;
    size_t i = 0;
    if (len >= part_len) {
        memcpy(&ctx->buffer[idx], data, part_len);
        sha512_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 127 < len; i += 128) {
            sha512_transform(ctx->state, &data[i]);
        }
        idx = 0;
    }
    memcpy(&ctx->buffer[idx], &data[i], len - i);
}

static void sha512_final(uint8_t digest[64], sha512_ctx *ctx)
{
    uint8_t count_bytes[16];
    for (int i = 0; i < 8; i++) {
        count_bytes[i] = (uint8_t)(ctx->count[1] >> (56 - i * 8));
        count_bytes[i + 8] = (uint8_t)(ctx->count[0] >> (56 - i * 8));
    }

    size_t idx = (size_t)((ctx->count[0] >> 3) & 127);
    size_t pad_len = (idx < 112) ? (112 - idx) : (240 - idx);
    static const uint8_t padding[128] = { 0x80 };
    sha512_update(ctx, padding, pad_len);
    sha512_update(ctx, count_bytes, 16);

    for (int i = 0; i < 8; i++) {
        digest[i * 8 + 0] = (uint8_t)(ctx->state[i] >> 56);
        digest[i * 8 + 1] = (uint8_t)(ctx->state[i] >> 48);
        digest[i * 8 + 2] = (uint8_t)(ctx->state[i] >> 40);
        digest[i * 8 + 3] = (uint8_t)(ctx->state[i] >> 32);
        digest[i * 8 + 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 8 + 5] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 8 + 6] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 8 + 7] = (uint8_t)(ctx->state[i]);
    }
}

static const char b64_table[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void b64_from_24bit(uint8_t b2, uint8_t b1, uint8_t b0, int n, char **cp)
{
    uint32_t w = ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | b0;
    while (n-- > 0) {
        **cp = b64_table[w & 0x3f];
        (*cp)++;
        w >>= 6;
    }
}

char *crypt(const char *key, const char *salt)
{
    static char output[128];
    if (!key || !salt) return NULL;

    /* Verify $6$ prefix */
    if (strncmp(salt, "$6$", 3) != 0) {
        return NULL;
    }

    const char *salt_start = salt + 3;
    unsigned long rounds = 5000;
    if (strncmp(salt_start, "rounds=", 7) == 0) {
        rounds = strtoul(salt_start + 7, (char **)&salt_start, 10);
        if (*salt_start == '$') salt_start++;
        if (rounds < 1000) rounds = 1000;
        if (rounds > 999999999) rounds = 999999999;
    }

    /* Extract salt characters up to next '$' or 16 chars */
    char clean_salt[17];
    size_t salt_len = 0;
    while (salt_start[salt_len] && salt_start[salt_len] != '$' && salt_len < 16) {
        clean_salt[salt_len] = salt_start[salt_len];
        salt_len++;
    }
    clean_salt[salt_len] = '\0';
    size_t key_len = strlen(key);

    /* 1. Alternate digest B */
    sha512_ctx alt_ctx;
    sha512_init(&alt_ctx);
    sha512_update(&alt_ctx, key, key_len);
    sha512_update(&alt_ctx, clean_salt, salt_len);
    sha512_update(&alt_ctx, key, key_len);
    uint8_t alt_result[64];
    sha512_final(alt_result, &alt_ctx);

    /* 2. Main digest A */
    sha512_ctx main_ctx;
    sha512_init(&main_ctx);
    sha512_update(&main_ctx, key, key_len);
    sha512_update(&main_ctx, clean_salt, salt_len);

    for (size_t cnt = key_len; cnt > 64; cnt -= 64) {
        sha512_update(&main_ctx, alt_result, 64);
    }
    sha512_update(&main_ctx, alt_result, key_len % 64);

    for (size_t cnt = key_len; cnt > 0; cnt >>= 1) {
        if ((cnt & 1) != 0) {
            sha512_update(&main_ctx, alt_result, 64);
        } else {
            sha512_update(&main_ctx, key, key_len);
        }
    }
    uint8_t p_bytes[64];
    sha512_final(p_bytes, &main_ctx);

    /* 3. Sequence P */
    sha512_init(&alt_ctx);
    for (size_t i = 0; i < key_len; i++) {
        sha512_update(&alt_ctx, key, key_len);
    }
    uint8_t p_seq[64];
    sha512_final(p_seq, &alt_ctx);
    uint8_t *p = malloc(key_len);
    if (!p) return NULL;
    for (size_t cp = 0; cp < key_len; cp++) {
        p[cp] = p_seq[cp % 64];
    }

    /* 4. Sequence S */
    sha512_init(&alt_ctx);
    for (size_t i = 0; i < (size_t)(16 + p_bytes[0]); i++) {
        sha512_update(&alt_ctx, clean_salt, salt_len);
    }
    uint8_t s_seq[64];
    sha512_final(s_seq, &alt_ctx);
    uint8_t *s = malloc(salt_len);
    if (!s) { free(p); return NULL; }
    for (size_t cp = 0; cp < salt_len; cp++) {
        s[cp] = s_seq[cp % 64];
    }

    /* 5. Repeated hashing loop */
    for (unsigned long i = 0; i < rounds; i++) {
        sha512_init(&alt_ctx);
        if ((i & 1) != 0) {
            sha512_update(&alt_ctx, p, key_len);
        } else {
            sha512_update(&alt_ctx, p_bytes, 64);
        }

        if (i % 3 != 0) {
            sha512_update(&alt_ctx, s, salt_len);
        }
        if (i % 7 != 0) {
            sha512_update(&alt_ctx, p, key_len);
        }

        if ((i & 1) != 0) {
            sha512_update(&alt_ctx, p_bytes, 64);
        } else {
            sha512_update(&alt_ctx, p, key_len);
        }
        sha512_final(p_bytes, &alt_ctx);
    }

    free(p);
    free(s);

    /* 6. Base64 encoding into output */
    char *cp = output;
    if (rounds != 5000) {
        cp += sprintf(cp, "$6$rounds=%lu$%s$", rounds, clean_salt);
    } else {
        cp += sprintf(cp, "$6$%s$", clean_salt);
    }

    static const int perm[21][3] = {
        {0, 21, 42}, {22, 43, 1}, {44, 2, 23}, {3, 24, 45}, {25, 46, 4},
        {47, 5, 26}, {6, 27, 48}, {28, 49, 7}, {50, 8, 29}, {9, 30, 51},
        {31, 52, 10}, {53, 11, 32}, {12, 33, 54}, {34, 55, 13}, {56, 14, 35},
        {15, 36, 57}, {37, 58, 16}, {59, 17, 38}, {18, 39, 60}, {40, 61, 19},
        {62, 20, 41}
    };

    for (int i = 0; i < 21; i++) {
        b64_from_24bit(p_bytes[perm[i][0]], p_bytes[perm[i][1]], p_bytes[perm[i][2]], 4, &cp);
    }
    b64_from_24bit(0, 0, p_bytes[63], 2, &cp);
    *cp = '\0';

    return output;
}
