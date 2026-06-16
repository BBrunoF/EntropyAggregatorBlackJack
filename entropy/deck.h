#ifndef DECK_H
#define DECK_H

#include <stdint.h>
#include "sha256.h"

#define DECK_SIZE 52

/* Decodes a 256-bit digest into a full 52-card permutation using the
 * factorial number system (Lehmer code). The digest is treated as one big
 * integer N; dividing it successively by 1,2,...,52 yields the Lehmer
 * digits, which select cards from the remaining set.
 *
 * out[] is filled with card indices 0..51 (rank = idx % 13, suit = idx / 13).
 * Modulo bias is ~1 in 1.4 billion (2^256 >> 52!) and is ignored. */
void deck_shuffle_from_digest(const uint8_t digest[SHA256_DIGEST_SIZE],
                              uint8_t out[DECK_SIZE]);

/* Prints the deck as rank+suit pairs, 13 per line. */
void deck_print(const uint8_t deck[DECK_SIZE]);

#endif
