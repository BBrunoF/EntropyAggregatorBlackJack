#ifndef BLACKJACK_H
#define BLACKJACK_H

#include <stdint.h>
#include <stdbool.h>

/* Pure BlackJack rules -- no graphics, no I/O, no globals. A "card" is 0..51
 * (rank = card % 13 with 0=Ace..12=King; suit = card / 13). Easy to unit-test. */

#define HAND_MAX 12

typedef struct {
    uint8_t cards[HAND_MAX];
    uint8_t count;
} Hand;

typedef enum {
    RESULT_PLAYER_BUST,
    RESULT_DEALER_BUST,
    RESULT_PLAYER_WIN,
    RESULT_DEALER_WIN,
    RESULT_PUSH
} Outcome;

void        hand_reset(Hand *h);
void        hand_add(Hand *h, uint8_t card);
int         hand_value(const Hand *h);            /* best total, aces 11 or 1 */
bool        hand_is_blackjack(const Hand *h);     /* natural 21: exactly 2 cards */
bool        dealer_should_hit(const Hand *h);     /* true while value < 17 */
Outcome     blackjack_settle(const Hand *player, const Hand *dealer);
const char *outcome_text(Outcome o);

#endif
