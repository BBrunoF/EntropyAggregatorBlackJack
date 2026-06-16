#ifndef ANALYSIS_H
#define ANALYSIS_H

/* Headless deck-bias analysis. Subscribes timer + keyboard + RTC (no graphics),
 * feeding the same entropy sources the live game does (mouse aside), and
 * shuffles decks continuously from live entropy, accumulating statistics:
 *   - per-rank frequency (each rank should converge to 1/13),
 *   - adjacency correlation: P(next high | current high) vs P(high), which
 *     directly tests "do high cards cluster together?".
 * Dumps a running report to the log every few thousand decks. ESC stops it.
 *
 * Triggered from proj_main_loop when the program is run with the "analyze"
 * argument (e.g. `lcom_run proj analyze`). */
int analysis_run(void);

#endif
