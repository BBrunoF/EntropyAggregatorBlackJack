#include <lcom/lcf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "analysis.h"
#include "kbc.h"
#include "rtc.h"
#include "entropy.h"
#include "deck.h"
#include "sha256.h"

#define SHUFFLES_PER_TICK 16      /* decks generated each timer interrupt */
#define REPORT_EVERY      5000    /* dump a report every N decks */
#define DEAL_DEPTH        12      /* only the first cards matter to blackjack */

/* "High" = a 10-value card: rank index 9..12 = T, J, Q, K (16 of 52). */
#define IS_HIGH(card) (((card) % 13) >= 9)

static uint64_t total_decks;
static uint64_t dealt_rank[13];        /* rank freq over the first DEAL_DEPTH */
static uint64_t pos_high[DEAL_DEPTH];  /* high-card count at each dealt position */
static uint64_t adj[2][2];             /* adjacency within the dealt region */

static void analyze_deck(const uint8_t deck[DECK_SIZE]) {
    for (int i = 0; i < DEAL_DEPTH; i++) {
        int rank = deck[i] % 13;
        dealt_rank[rank]++;
        if (IS_HIGH(deck[i]))
            pos_high[i]++;
        if (i + 1 < DEAL_DEPTH)
            adj[IS_HIGH(deck[i])][IS_HIGH(deck[i + 1])]++;
    }
    total_decks++;
}

/* Print num/den as a percentage with two decimals, integer math only. */
static void print_pct(uint64_t num, uint64_t den) {
    uint64_t bp = den ? (num * 10000u + den / 2) / den : 0;   /* hundredths of % */
    printf("%llu.%02llu%%", (unsigned long long) (bp / 100),
                            (unsigned long long) (bp % 100));
}

static void report(void) {
    static const char ranks[] = "A23456789TJQK";
    uint64_t dealt = total_decks * (uint64_t) DEAL_DEPTH;

    printf("\n===== deck analysis: %llu decks, first %d cards each =====\n",
           (unsigned long long) total_decks, DEAL_DEPTH);

    /* Aggregate rank frequency over the dealt cards (NOT the whole deck, which
     * would be uniform by construction). Each rank should sit near 7.69%. */
    printf("rank freq over dealt cards (expect 7.69%% each):\n");
    for (int r = 0; r < 13; r++) {
        printf("  %c %10llu  ", ranks[r], (unsigned long long) dealt_rank[r]);
        print_pct(dealt_rank[r], dealt);
        printf("\n");
    }

    /* Per-position high-card rate: this is where a biased early shuffle shows.
     * Uniform => every position ~30.77%. */
    printf("high-card (T/J/Q/K) rate per position (expect 30.77%%):\n");
    for (int i = 0; i < DEAL_DEPTH; i++) {
        printf("  pos %2d  ", i);
        print_pct(pos_high[i], total_decks);
        printf("\n");
    }

    /* Does a high dealt card tend to be followed by another high dealt card? */
    uint64_t highs    = dealt_rank[9] + dealt_rank[10] + dealt_rank[11] + dealt_rank[12];
    uint64_t cur_high = adj[1][0] + adj[1][1];
    uint64_t cur_low  = adj[0][0] + adj[0][1];
    printf("dealt-region correlation:\n");
    printf("  P(high)               = ");
    print_pct(highs, dealt);
    printf("\n  P(next high | cur high) = ");
    print_pct(adj[1][1], cur_high);
    printf("\n  P(next high | cur low)  = ");
    print_pct(adj[0][1], cur_low);
    printf("\n(uniform shuffle => these three are ~equal)\n");
}

int analysis_run(void) {
    entropy_init();

    uint8_t timer_bit = 0, kbd_bit = 0;
    if (timer_subscribe_int(&timer_bit) != OK) {
        printf("analyze: timer_subscribe_int failed\n");
        return 1;
    }
    if (kbc_subscribe_int(&kbd_bit) != OK) {
        printf("analyze: kbc_subscribe_int failed\n");
        timer_unsubscribe_int();
        return 1;
    }

    /* RTC is optional, exactly as in the live game (main.c): if it fails to
     * subscribe we keep sampling without it rather than aborting the run. */
    uint8_t rtc_bit = 0;
    bool    rtc_on  = false;
    if (rtc_subscribe_int(&rtc_bit) == OK && rtc_enable_updates() == OK) {
        rtc_on = true;
    } else {
        rtc_unsubscribe_int();
        printf("analyze: continuing without RTC entropy\n");
    }

    const uint32_t timer_mask = BIT(timer_bit);
    const uint32_t kbd_mask   = BIT(kbd_bit);
    const uint32_t rtc_mask   = rtc_on ? BIT(rtc_bit) : 0;

    int      ipc_status;
    message  msg;
    bool     done = false;
    uint64_t next_report = REPORT_EVERY;

    printf("\n=== deck analysis (headless) -- ESC to stop ===\n");
    printf("Sampling the first %d cards of %d decks/tick from live entropy.\n",
           DEAL_DEPTH, SHUFFLES_PER_TICK);

    while (!done) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0)
            continue;
        if (!is_ipc_notify(ipc_status))
            continue;
        if (_ENDPOINT_P(msg.m_source) != HARDWARE)
            continue;

        if (msg.m_notify.interrupts & timer_mask) {
            timer_int_handler();
            entropy_feed_timer();

            if (entropy_ready()) {
                uint8_t commit[SHA256_DIGEST_SIZE];
                uint8_t deck[DECK_SIZE];
                for (int b = 0; b < SHUFFLES_PER_TICK; b++) {
                    entropy_commit(commit);            /* fresh seed, folds back */
                    entropy_shuffle_committed(deck);   /* decode to a deck */
                    analyze_deck(deck);
                }
                if (total_decks >= next_report) {
                    report();
                    next_report += REPORT_EVERY;
                }
            }
        }

        if (rtc_on && (msg.m_notify.interrupts & rtc_mask)) {
            rtc_ih();
            uint8_t rb[4];
            int n = rtc_get_entropy(rb, 4);
            entropy_feed_rtc(rb, n);
        }

        if (msg.m_notify.interrupts & kbd_mask) {
            kbc_ih();
            if (!kbc_get_error()) {
                uint8_t sc = kbc_get_scancode_byte();
                entropy_feed_kbd(sc);
                if (sc == ESC_BREAK)
                    done = true;
            }
        }
    }

    report();   /* final tally */
    if (rtc_on) {
        rtc_disable_updates();
        rtc_unsubscribe_int();
    }
    kbc_unsubscribe_int();
    timer_unsubscribe_int();
    printf("\nanalysis stopped.\n");
    return 0;
}
