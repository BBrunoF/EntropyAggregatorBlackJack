#include "pool.h"

void pool_init(entropy_pool *p) {
    sha256_init(&p->ctx);
    p->events = 0;
    for (int i = 0; i < POOL_SRC_COUNT; i++)
        p->src_events[i] = 0;
}

void pool_mix(entropy_pool *p, pool_source_t src,
              const uint8_t *bytes, size_t n, uint16_t pit_count) {
    uint8_t hdr[3];
    hdr[0] = (uint8_t) src;
    hdr[1] = (uint8_t) (pit_count & 0xFF);
    hdr[2] = (uint8_t) ((pit_count >> 8) & 0xFF);

    sha256_update(&p->ctx, hdr, sizeof hdr);
    if (n > 0 && bytes != NULL)
        sha256_update(&p->ctx, bytes, n);

    p->events++;
    if ((int) src < POOL_SRC_COUNT)
        p->src_events[src]++;
}

void pool_extract(entropy_pool *p, uint8_t out[SHA256_DIGEST_SIZE]) {
    sha256_ctx clone = p->ctx;        /* shallow copy of the running context */
    sha256_final(&clone, out);
    sha256_update(&p->ctx, out, SHA256_DIGEST_SIZE);
}

uint64_t pool_event_count(const entropy_pool *p) {
    return p->events;
}

uint64_t pool_source_count(const entropy_pool *p, pool_source_t src) {
    return ((int) src < POOL_SRC_COUNT) ? p->src_events[src] : 0;
}
