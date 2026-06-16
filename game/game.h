#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

#include "blackjack.h"
#include "sprite.h"
#include "button.h"
#include "deck.h"      /* DECK_SIZE */
#include "mode.h"      /* GameMode */

typedef enum {
    STATE_MENU,
    STATE_RULES,
    STATE_BET,        /* player picks a wager before the hand is dealt */
    STATE_DEAL,       /* dealer's hand places the chips, then cards are dealt */
    STATE_PLAYER,
    STATE_DEALER,
    STATE_RESULT,     /* WIN/LOSE/PUSH flashes, then auto-advances */
    STATE_GAMEOVER    /* one side is broke -> GAME WON / GAME LOST */
} GameState;

/* Deal animation: cards slide from the centre of the table to their hand
 * positions over this many timer ticks (~0.5 s at 60 Hz). */
#define ANIM_DURATION 30

/* A dealer's hand rises from below, places the wager chips, and retracts over
 * this many ticks BEFORE the cards are dealt (extend / open / retract phases).
 * Deliberately slow so the gesture reads clearly. */
#define CHIP_ANIM_DURATION 100

/* Betting bounds (shared with render for the HUD hints). */
#define MIN_BET   5
#define BET_STEP  5

/* Shadow-dealer typewriter speed (ticks per revealed character) -- shared so
 * render can compute how many characters to show. */
#define DEALER_TYPE_DIV 2

/* Dealer claw chip-snatch duration (ticks) -- shared with render for phasing. */
#define SNATCH_DURATION 66

/* PURGATORY menu glitch: total ticks. Flickers in/out at the edges and holds
 * solid (readable) in the middle. Shared so render can phase it. */
#define CURSED_FLASH_LEN 60

/* The whole game in one struct. Only game.c mutates it; render.c reads it. */
typedef struct {
    GameState state;
    Hand      player, dealer;
    uint8_t   deck[DECK_SIZE];
    int       deck_index;
    int       dealer_tick;     /* paces the dealer's auto-draw */
    Outcome   outcome;

    int       balance;         /* player's money */
    int       dealer_balance;  /* the house's money */
    int       bet;             /* wager for the current hand */
    int       round;           /* hands played so far */

    int       result_flash;    /* >0 while the WIN/LOSE banner is showing */
    int       chip_anim;       /* >0 while the wager chips slide onto the table */
    int       over_tick;       /* counts up on the GAME OVER screen */
    bool      game_won;        /* GAME WON (true) vs GAME LOST (false) */

    /* Active ruleset (Normal vs Purgatory), chosen at the menu. Every
     * difference between the two lives in the GameMode profile (see mode.h);
     * the game flow just reads g.mode->finite_house / g.mode->horror / its
     * dialogue pools at the points where the modes diverge. */
    const GameMode *mode;

    /* Creepy shadow-dealer overlay (appears suddenly, talks, then vanishes). */
    bool        dealer_show;   /* eyes + speech box visible */
    const char *dealer_line;   /* current line of dialogue */
    int         dealer_chars;  /* typewriter progress, in ticks */
    int         dealer_hold;   /* ticks to linger after the line is fully typed */

    /* Dealer's claw snatching the chips away on a loss. */
    int       snatch_anim;     /* >0 during the snatch; counts down */
    bool      chips_taken;     /* table chips have been grabbed */

    /* Dealer's arm (mirrored) snatching chips when the dealer wins. */
    int       dealer_win_snatch;

    int       cursed_flash;    /* >0 -> menu briefly glitches to PURGATORY */
    int       dread;           /* escalating horror level (rises as you lose) */

    int       anim_counter;            /* >0 while deal animation runs; counts down */
    uint8_t   anim_settled_player;     /* cards at index < this are at final position */
    uint8_t   anim_settled_dealer;     /* (during mid-deal-hit/dealer-draw animation) */

    uint16_t  cursor_x, cursor_y;
    uint16_t  prev_cursor_x, prev_cursor_y;  /* last drawn cursor (dirty-rect) */
    bool      dirty;                          /* scene changed -> full flush */

    Sprite   *cursor;
    Sprite   *card_face, *card_back, *chip;
    Sprite   *hand_fist, *hand_open;   /* player-side hand delivering the wager */
    Sprite   *dealer_claw, *dealer_fist, *dealer_eyes;  /* shadow dealer */
    Button   *play, *rules, *exit;            /* menu */
    Button   *skin_btn;                             /* table skin selector */
    Button   *hit, *stand;                    /* player turn */
    Button   *bet_minus, *bet_plus, *allin, *deal;  /* betting */
    Button   *menu_btn;                             /* back to menu (bet screen) */
    bool      exit_requested;

    int       rules_page;          /* 0 or 1 for multi-page rules */
    int       skin_index;
    char      skin_label[24];
} Game;

/* Lifecycle. video_init() must have run first (cursor/button sizes need it). */
int  game_init(void);
void game_free(void);
bool game_should_exit(void);

/* Driven by the main interrupt loop. */
void game_handle_timer(void);            /* advance dealer animation + flashes */
void game_handle_mouse(int dx, int dy);  /* move cursor + update hovers */
void game_handle_click(void);            /* left-button press at cursor */
void game_handle_key(uint8_t scancode);  /* keyboard input */
void game_render(void);                  /* compose + flush current frame */

#endif
