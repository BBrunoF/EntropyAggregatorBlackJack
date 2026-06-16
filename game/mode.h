#ifndef MODE_H
#define MODE_H

#include <stdbool.h>

/* ── Game modes: Normal vs Purgatory ──────────────────────────────────────
 * A GameMode captures the COMPLETE set of differences between the two ways to
 * play, in one place. The shared game flow ("the spine", in game.c) reads these
 * fields at each point where the modes diverge; everything else is identical.
 *
 *   Normal     -> plain casino blackjack: infinite house, no horror flavor.
 *   Purgatory  -> finite dealer bankroll (drainable -> GAME WON) plus the
 *                 shadow-dealer eyes, dialogue, and chip-snatching claw.
 *
 * Purgatory is the hidden easter egg, entered by hitting PLAY during the menu's
 * cursed flash (see game.c game_handle_click / game_handle_key). */

/* A rotating pool of shadow-dealer lines. Empty ({0,0}) in Normal. */
typedef struct {
    const char **lines;
    int          count;
} LinePool;

typedef struct {
    const char *name;          /* for logs */
    bool        finite_house;  /* dealer pot drains -> bankruptable (GAME WON) */
    bool        horror;        /* eyes + dialogue + claw + creepy game-over */

    /* Dialogue pools, used only when `horror`. */
    LinePool    intro;         /* on a fresh game */
    LinePool    player_win;    /* player beats the dealer / dealer busts */
    LinePool    bust;          /* player busts */
    LinePool    dealer_win;    /* dealer wins outright */
    LinePool    push;          /* a draw */
    LinePool    over_won;      /* GAME WON screen */
    LinePool    over_lost;     /* GAME LOST screen */
    LinePool    allin;         /* reactive: player bets everything */
    LinePool    low;           /* reactive: player is nearly broke */
} GameMode;

extern const GameMode MODE_NORMAL;
extern const GameMode MODE_PURGATORY;

/* Next line from a pool (round-robins across calls; "" for an empty pool). */
const char *mode_line(LinePool pool);

#endif
