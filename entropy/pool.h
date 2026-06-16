#ifndef POOL_H
#define POOL_H

#include <stdint.h>
#include <stddef.h>
#include "sha256.h"

typedef enum {
    POOL_SRC_TIMER = 0,
    POOL_SRC_KBD   = 1,
    POOL_SRC_MOUSE = 2,
    POOL_SRC_RTC   = 3,
} pool_source_t;

#define POOL_SRC_COUNT 4

typedef struct {
    sha256_ctx ctx;
    uint64_t   events;
    uint64_t   src_events[POOL_SRC_COUNT];   /* events per source */
} entropy_pool;

void     pool_init(entropy_pool *p);

/* Mix one event into the pool. `bytes` may be NULL if n == 0.
 * `pit_count` is a fine-grained timestamp (PIT counter 0); the
 * inter-arrival jitter is the dominant entropy source on top of
 * the raw payload bytes. */
void     pool_mix(entropy_pool *p, pool_source_t src,
                  const uint8_t *bytes, size_t n, uint16_t pit_count);

/* Extracts a 32-byte digest of the current pool state. The digest
 * is folded back into the pool so subsequent extractions are not
 * trivially correlated with this one. */
void     pool_extract(entropy_pool *p, uint8_t out[SHA256_DIGEST_SIZE]);

uint64_t pool_event_count(const entropy_pool *p);
uint64_t pool_source_count(const entropy_pool *p, pool_source_t src);

#endif
