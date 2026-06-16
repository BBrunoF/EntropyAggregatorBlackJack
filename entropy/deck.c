#include <lcom/lcf.h>   /* redirects printf to the lcf_log_output sink */
#include <stdio.h>
#include "deck.h"

/* Divide a big-endian 256-bit number (32 bytes) by a small divisor d (<=52)
 * in place, returning the remainder. Classic base-256 long division: walk
 * from the most-significant byte, carrying the remainder forward. Since
 * d < 256, the accumulator (rem << 8 | byte) always fits in a uint32_t. */
static uint8_t bignum_divmod_small(uint8_t n[SHA256_DIGEST_SIZE], uint8_t d) {
    uint32_t rem = 0;
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        uint32_t acc = (rem << 8) | n[i];
        n[i] = (uint8_t) (acc / d);
        rem  = acc % d;
    }
    return (uint8_t) rem;
}

void deck_shuffle_from_digest(const uint8_t digest[SHA256_DIGEST_SIZE],
                              uint8_t out[DECK_SIZE]) {
    /* Work on a mutable copy of the digest (our big integer N). */
    uint8_t N[SHA256_DIGEST_SIZE];
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
        N[i] = digest[i];

    /* Factoradic digits: lehmer[i-1] = N % i, then N /= i, for i = 1..52.
     * lehmer[k] is guaranteed to lie in 0..k. */
    uint8_t lehmer[DECK_SIZE];
    for (int i = 1; i <= DECK_SIZE; i++)
        lehmer[i - 1] = bignum_divmod_small(N, (uint8_t) i);

    /* Remaining cards, initially sorted 0..51. */
    uint8_t remaining[DECK_SIZE];
    for (int i = 0; i < DECK_SIZE; i++)
        remaining[i] = (uint8_t) i;
    int rem_count = DECK_SIZE;

    /* Decode from the highest place down: at place k the remaining set has
     * exactly k+1 cards, and lehmer[k] (0..k) picks one of them. */
    for (int k = DECK_SIZE - 1; k >= 0; k--) {
        uint8_t idx = lehmer[k];
        out[DECK_SIZE - 1 - k] = remaining[idx];
        /* Remove the chosen card by shifting the tail down. */
        for (int j = idx; j < rem_count - 1; j++)
            remaining[j] = remaining[j + 1];
        rem_count--;
    }
}

void deck_print(const uint8_t deck[DECK_SIZE]) {
    static const char ranks[] = "A23456789TJQK";
    static const char suits[] = "SHDC";
    for (int i = 0; i < DECK_SIZE; i++) {
        uint8_t c = deck[i];
        printf("%c%c ", ranks[c % 13], suits[c / 13]);
        if ((i + 1) % 13 == 0)
            printf("\n");
    }
    printf("\n");
}
