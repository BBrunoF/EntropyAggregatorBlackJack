#ifndef ENTROPY_H
#define ENTROPY_H

#include <stdint.h>
#include <stdbool.h>
#include "sha256.h"   /* SHA256_DIGEST_SIZE */
#include "deck.h"     /* DECK_SIZE          */

/* ---------------------------------------------------------------------------
 * Entropy facade.
 *
 * This is the ONLY interface the game/main loop should touch. It hides the
 * SHA-256 pool, the Lehmer decode, and the commit/reveal bookkeeping.
 *
 * Integration contract (important): the pool only stays healthy if the game's
 * central interrupt loop calls the entropy_feed_* function on EVERY matching
 * device interrupt. Skip that and the pool silently starves -- no error, just
 * weak, near-repeating shuffles. So in your driver_receive loop:
 *
 *     timer IRQ  -> entropy_feed_timer();
 *     kbd   IRQ  -> entropy_feed_kbd(scancode);
 *     mouse IRQ  -> entropy_feed_mouse(byte);
 *
 * Provably-fair hand (commit/reveal):
 *     1. entropy_commit(commit)        -> display `commit` BEFORE dealing
 *     2. entropy_shuffle_committed(deck)-> deal from this deck (seed stays secret)
 *     3. ...play the hand...
 *     4. entropy_reveal(seed)          -> display `seed` AFTER the hand
 *     Anyone can then check  SHA256(seed) == commit  and  Lehmer(seed) == deck.
 * ------------------------------------------------------------------------- */

void     entropy_init(void);

/* Feed one device event into the pool. Each call timestamps the event with
 * the live PIT counter internally -- callers just pass the payload byte(s). */
void     entropy_feed_timer(void);
void     entropy_feed_kbd(uint8_t scancode);
void     entropy_feed_mouse(uint8_t byte);

/* Mix RTC payload bytes (time registers captured at an RTC interrupt) into the
 * pool, timestamped with the live PIT counter like the other sources. */
void     entropy_feed_rtc(const uint8_t *bytes, int n);

/* True once enough events have accumulated for a trustworthy first deal. */
bool     entropy_ready(void);

/* Event counts split by source, written in order: timer, kbd, mouse, rtc.
 * Lets a caller see exactly how much each device is contributing. */
void     entropy_event_breakdown(uint64_t counts[4]);

/* Total events absorbed so far (for a HUD / debugging). */
uint64_t entropy_events(void);

/* Start a hand: pull a fresh, independent seed from the pool, keep it secret,
 * and return commit = SHA256(seed). Show `commit` before any card is dealt. */
void     entropy_commit(uint8_t commit_out[SHA256_DIGEST_SIZE]);

/* Decode the committed seed into a full 52-card deck (Lehmer). Does NOT
 * reveal the seed. Call after entropy_commit, when dealing. */
void     entropy_shuffle_committed(uint8_t deck_out[DECK_SIZE]);

/* Reveal the committed seed -- call only AFTER the hand is over. */
void     entropy_reveal(uint8_t seed_out[SHA256_DIGEST_SIZE]);

#endif
