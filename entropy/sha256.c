#include "sha256.h"

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n)  ((x) >> (n))

#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#define SIG0(x) (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIG1(x) (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sig0(x) (ROTR(x,  7) ^ ROTR(x, 18) ^  SHR(x,  3))
#define sig1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^  SHR(x, 10))

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

void sha256_init(sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buflen = 0;

    for (int i = 0; i < SHA256_BLOCK_SIZE; i++)
        ctx->buffer[i] = 0;
}

static void sha256_transform(sha256_ctx *ctx, const uint8_t block[SHA256_BLOCK_SIZE]) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    int i;

    /* Build message schedule */
    for (i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i*4    ] << 24) |
               ((uint32_t)block[i*4 + 1] << 16) |
               ((uint32_t)block[i*4 + 2] <<  8) |
               ((uint32_t)block[i*4 + 3]      );
    }
    for (i = 16; i < 64; i++) {
        W[i] = sig1(W[i-2]) + W[i-7] + sig0(W[i-15]) + W[i-16];
    }

    /* Initialize working variables */
    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    /* 64 rounds */
    for (i = 0; i < 64; i++) {
        T1 = h + SIG1(e) + CH(e,f,g) + K[i] + W[i];
        T2 = SIG0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }

    /* Add to current hash state */
    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    
    ctx->bitcount += (uint64_t)len * 8;

    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];

        if (ctx->buflen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buflen = 0;
        }
    }    
}

void sha256_final(sha256_ctx *ctx, uint8_t digest[SHA256_DIGEST_SIZE]) {
    int i;

    /* Append the 0x80 byte */
    ctx->buffer[ctx->buflen++] = 0x80;

    /* If no room for the 8-byte length, process and reset */
    if (ctx->buflen > 56) {
        while (ctx->buflen < SHA256_BLOCK_SIZE)
            ctx->buffer[ctx->buflen++] = 0x00;
        sha256_transform(ctx, ctx->buffer);
        ctx->buflen = 0;
    }

    /* Pad with zeros up to byte 56 */
    while (ctx->buflen < 56)
        ctx->buffer[ctx->buflen++] = 0x00;

    /* Append bitcount as 64-bit big-endian */
    ctx->buffer[56] = (ctx->bitcount >> 56) & 0xff;
    ctx->buffer[57] = (ctx->bitcount >> 48) & 0xff;
    ctx->buffer[58] = (ctx->bitcount >> 40) & 0xff;
    ctx->buffer[59] = (ctx->bitcount >> 32) & 0xff;
    ctx->buffer[60] = (ctx->bitcount >> 24) & 0xff;
    ctx->buffer[61] = (ctx->bitcount >> 16) & 0xff;
    ctx->buffer[62] = (ctx->bitcount >>  8) & 0xff;
    ctx->buffer[63] = (ctx->bitcount      ) & 0xff;

    /* Process final block */
    sha256_transform(ctx, ctx->buffer);

    /* Serialize state into digest, big-endian */
    for (i = 0; i < 8; i++) {
        digest[i*4    ] = (ctx->state[i] >> 24) & 0xff;
        digest[i*4 + 1] = (ctx->state[i] >> 16) & 0xff;
        digest[i*4 + 2] = (ctx->state[i] >>  8) & 0xff;
        digest[i*4 + 3] = (ctx->state[i]      ) & 0xff;
    }
}

void sha256(const uint8_t *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}
