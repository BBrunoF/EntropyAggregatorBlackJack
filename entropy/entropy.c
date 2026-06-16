#include "entropy.h"
#include "pool.h"
#include "timer_entropy.h"

#define MIN_ENTROPY_EVENTS  64   /* freshness gate before the first deal */

static entropy_pool pool;
static uint8_t      pending_seed[SHA256_DIGEST_SIZE];
static bool         have_seed = false;

void entropy_init(void) {
    pool_init(&pool);
    have_seed = false;
}

void entropy_feed_timer(void) {
    uint16_t pit = timer_read_count();
    uint8_t  bytes[2] = { (uint8_t) (pit & 0xFF), (uint8_t) ((pit >> 8) & 0xFF) };
    pool_mix(&pool, POOL_SRC_TIMER, bytes, sizeof bytes, pit);
}

void entropy_feed_kbd(uint8_t scancode) {
    uint16_t pit = timer_read_count();
    pool_mix(&pool, POOL_SRC_KBD, &scancode, 1, pit);
}

void entropy_feed_mouse(uint8_t byte) {
    uint16_t pit = timer_read_count();
    pool_mix(&pool, POOL_SRC_MOUSE, &byte, 1, pit);
}

void entropy_feed_rtc(const uint8_t *bytes, int n) {
    uint16_t pit = timer_read_count();
    pool_mix(&pool, POOL_SRC_RTC, bytes, (size_t) n, pit);
}

bool entropy_ready(void) {
    return pool_event_count(&pool) >= MIN_ENTROPY_EVENTS;
}

uint64_t entropy_events(void) {
    return pool_event_count(&pool);
}

void entropy_event_breakdown(uint64_t counts[4]) {
    counts[0] = pool_source_count(&pool, POOL_SRC_TIMER);
    counts[1] = pool_source_count(&pool, POOL_SRC_KBD);
    counts[2] = pool_source_count(&pool, POOL_SRC_MOUSE);
    counts[3] = pool_source_count(&pool, POOL_SRC_RTC);
}

void entropy_commit(uint8_t commit_out[SHA256_DIGEST_SIZE]) {
    /* Fresh, independent seed (pool_extract folds its output back in). */
    pool_extract(&pool, pending_seed);
    have_seed = true;
    /* Commitment is the hash of the seed: reveals nothing about the seed
     * (preimage resistance), but binds us to it. */
    sha256(pending_seed, SHA256_DIGEST_SIZE, commit_out);
}

void entropy_shuffle_committed(uint8_t deck_out[DECK_SIZE]) {
    /* Defensive: if a deck is requested without a prior commit, pull a seed
     * now so callers never get an uninitialized deck. */
    if (!have_seed) {
        pool_extract(&pool, pending_seed);
        have_seed = true;
    }
    deck_shuffle_from_digest(pending_seed, deck_out);
}

void entropy_reveal(uint8_t seed_out[SHA256_DIGEST_SIZE]) {
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
        seed_out[i] = pending_seed[i];
}
