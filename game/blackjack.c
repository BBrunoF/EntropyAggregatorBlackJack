#include "blackjack.h"

void hand_reset(Hand *h) {
    h->count = 0;
}

void hand_add(Hand *h, uint8_t card) {
    if (h->count < HAND_MAX)
        h->cards[h->count++] = card;
}

int hand_value(const Hand *h) {
    int total = 0;
    int aces  = 0;

    for (uint8_t i = 0; i < h->count; i++) {
        int rank = h->cards[i] % 13;   /* 0=A, 1..8 = 2..9, 9..12 = 10/J/Q/K */
        if (rank == 0) {
            total += 11;
            aces++;
        } else if (rank >= 9) {
            total += 10;
        } else {
            total += rank + 1;
        }
    }

    /* Demote aces from 11 to 1 while we're over 21. */
    while (total > 21 && aces > 0) {
        total -= 10;
        aces--;
    }
    return total;
}

bool hand_is_blackjack(const Hand *h) {
    return h->count == 2 && hand_value(h) == 21;
}

bool dealer_should_hit(const Hand *h) {
    /* Dealer draws to 17 and stands on all 17s (including soft 17). */
    return hand_value(h) < 17;
}

Outcome blackjack_settle(const Hand *player, const Hand *dealer) {
    int p = hand_value(player);
    int d = hand_value(dealer);

    /* A natural (two-card 21) outranks any drawn 21. */
    bool pbj = hand_is_blackjack(player);
    bool dbj = hand_is_blackjack(dealer);
    if (pbj || dbj) {
        if (pbj && dbj) return RESULT_PUSH;
        return pbj ? RESULT_PLAYER_WIN : RESULT_DEALER_WIN;
    }

    if (p > 21) return RESULT_PLAYER_BUST;
    if (d > 21) return RESULT_DEALER_BUST;
    if (p > d)  return RESULT_PLAYER_WIN;
    if (d > p)  return RESULT_DEALER_WIN;
    return RESULT_PUSH;
}

const char *outcome_text(Outcome o) {
    switch (o) {
        case RESULT_PLAYER_BUST: return "YOU BUST!";
        case RESULT_DEALER_BUST: return "DEALER BUST!";
        case RESULT_PLAYER_WIN:  return "YOU WIN!";
        case RESULT_DEALER_WIN:  return "DEALER WINS!";
        case RESULT_PUSH:        return "PUSH!";
    }
    return "";
}
