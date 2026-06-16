#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "game.h"
#include "render.h"
#include "video.h"
#include "text.h"
#include "assets.h"
#include "entropy.h"
#include "deck.h"
#include "sha256.h"

#define DEALER_DELAY       60    /* timer ticks between dealer draws (~1s at 60Hz) */
#define PLAYER_START       50    /* starting bankroll, player */
#define DEALER_START       50    /* starting bankroll, house */
#define RESULT_FLASH_TICKS 90    /* how long WIN/LOSE flashes (~1.5s) */
#define GAMEOVER_TICKS     240   /* auto-return to menu after GAME OVER (~4s) */
#define MOUSE_SENS         4     /* cursor pixels per unit of mouse delta */
#define DEALER_HOLD        110   /* ticks the dialogue lingers once fully typed */
/* DEALER_TYPE_DIV, SNATCH_DURATION, CURSED_FLASH_LEN live in game.h. */

/* Scancode make codes (scan-code set 1). */
#define KEY_SPACE     0x39
#define KEY_ENTER     0x1C
#define KEY_BACKSPACE 0x0E
#define KEY_UP        0x48
#define KEY_DOWN      0x50
#define KEY_KP_PLUS   0x4E
#define KEY_KP_MINUS  0x4A
#define KEY_T         0x14

static Game g;

static void enter_result(Outcome o);

/* Shadow-dealer dialogue + the two mode profiles live in mode.c; this just
 * shows one already-chosen line. Lines are pulled with mode_line(g.mode->...). */
static void dealer_say(const char *line) {
    g.dealer_show  = true;
    g.dealer_line  = line;
    g.dealer_chars = 0;
    g.dealer_hold  = DEALER_HOLD;
    g.dirty        = true;
}

static void apply_skin(int idx) {
    g.skin_index = ((idx % TABLE_SKIN_COUNT) + TABLE_SKIN_COUNT) % TABLE_SKIN_COUNT;
    render_set_skin(g.skin_index);
    sprintf(g.skin_label, "SKIN: %s", render_skin_name(g.skin_index));
    if (g.skin_btn != NULL)
        g.skin_btn->label = g.skin_label;
    render_bake_table();
    g.dirty = true;
}

/* Switch the baked backdrop between the normal skin and the cursed blood felt.
 * Re-bakes only when the world actually changes (baking is not cheap). */
static bool g_baked_cursed = false;
static void set_world(bool cursed) {
    if (cursed != g_baked_cursed) {
        render_set_cursed(cursed);
        render_bake_table();
        g_baked_cursed = cursed;
    }
}

/* Return to the menu, restoring the normal (non-cursed) backdrop. */
static void go_menu(void) {
    set_world(false);
    g.state = STATE_MENU;
    g.dirty = true;
}

/* ── Hand helpers ───────────────────────────────────────────────────────── */
static void deal_to(Hand *h) {
    if (g.deck_index < DECK_SIZE)
        hand_add(h, g.deck[g.deck_index++]);
}

/* Clamp the current bet into the legal range for the player's balance. If the
 * player can't even cover the minimum, they're forced all-in. */
static void clamp_bet(void) {
    if (g.balance < MIN_BET) {
        g.bet = g.balance;            /* forced all-in */
        return;
    }
    if (g.bet < MIN_BET)   g.bet = MIN_BET;
    if (g.bet > g.balance) g.bet = g.balance;
}

/* ── Round lifecycle ────────────────────────────────────────────────────── */

/* Deal a fresh hand from an entropy-seeded deck. The provably-fair seed still
 * drives the shuffle under the hood (commit pulls a fresh seed each hand); we
 * just no longer interrupt play to show it. */
static void start_hand(void) {
    uint8_t commit[SHA256_DIGEST_SIZE];

    g.dirty = true;

    entropy_commit(commit);                 /* fresh independent seed per hand */
    entropy_shuffle_committed(g.deck);
    printf("\n[Round %d] bet=%d  wallet=%d  dealer=%d\n",
           g.round, g.bet, g.balance, g.dealer_balance);

    hand_reset(&g.player);
    hand_reset(&g.dealer);
    g.deck_index = 0;
    deal_to(&g.player);
    deal_to(&g.dealer);
    deal_to(&g.player);
    deal_to(&g.dealer);

    g.anim_settled_player = 0;
    g.anim_settled_dealer = 0;
    g.anim_counter = ANIM_DURATION;
    g.snatch_anim  = 0;
    g.dealer_win_snatch = 0;
    g.chips_taken  = false;       /* fresh chips on the table for this hand */
    g.state = STATE_PLAYER;
}

/* Settle the wager between player and house, then flash the outcome. */
static void enter_result(Outcome o) {
    g.outcome = o;

    int delta = 0;   /* change to the player's wallet */
    switch (o) {
        case RESULT_PLAYER_WIN:
        case RESULT_DEALER_BUST:
            delta = hand_is_blackjack(&g.player) ? (g.bet * 3 / 2) : g.bet;
            break;
        case RESULT_PLAYER_BUST:
        case RESULT_DEALER_WIN:
            delta = -g.bet;
            break;
        case RESULT_PUSH:
            delta = 0;
            break;
    }

    /* The player can never lose more than they hold. In Purgatory the house is
     * a finite pot that also can't pay out more than it has; in Normal the
     * house is infinite, so only the player's wallet bounds the swing and the
     * dealer pot is left untouched. */
    if (delta < -g.balance) delta = -g.balance;
    if (g.mode->finite_house && delta > g.dealer_balance) delta = g.dealer_balance;

    g.balance += delta;
    if (g.mode->finite_house)
        g.dealer_balance -= delta;

    printf("[Result] %s  (%+d -> wallet=%d  dealer=%d)\n",
           outcome_text(o), delta, g.balance, g.dealer_balance);

    /* Chip choreography: the payout slide plays in both modes. The shadow
     * dealer's voice and the chip-snatching claw are horror-only flavor. */
    bool player_won = (o == RESULT_PLAYER_WIN || o == RESULT_DEALER_BUST);
    bool dealer_won = (o == RESULT_PLAYER_BUST || o == RESULT_DEALER_WIN);
    if (player_won) {
        g.chip_anim = CHIP_ANIM_DURATION;       /* payout slides in */
        if (g.mode->horror) {
            dealer_say(mode_line(g.mode->player_win));
            if (g.dread > 0) g.dread--;         /* a win eases the dread */
        }
    } else if (dealer_won) {
        if (g.mode->horror) {
            g.snatch_anim = SNATCH_DURATION;    /* claw grabs the chips */
            dealer_say(o == RESULT_PLAYER_BUST ? mode_line(g.mode->bust)
                                               : mode_line(g.mode->dealer_win));
            if (g.dread < 6) g.dread++;         /* losing deepens the horror */
        } else {
            g.dealer_win_snatch = CHIP_ANIM_DURATION; /* mirrored arm grabs right chips */
        }
    } else if (g.mode->horror) {
        dealer_say(mode_line(g.mode->push));
    }

    g.state        = STATE_RESULT;
    g.result_flash = RESULT_FLASH_TICKS;
    g.dirty        = true;
}

/* Called once the WIN/LOSE flash finishes: decide game over or next bet. */
static void advance_after_result(void) {
    if (g.balance <= 0) {
        g.game_won    = false;
        g.over_tick   = 0;
        g.state       = STATE_GAMEOVER;
        if (g.mode->horror) {
            g.dealer_line = mode_line(g.mode->over_lost);  /* shown by the lose sequence */
            g.dealer_show = false;                          /* no top box during the flash */
        }
        printf("[Game] GAME LOST after %d rounds\n", g.round);
    } else if (g.mode->finite_house && g.dealer_balance <= 0) {
        g.game_won  = true;
        g.over_tick = 0;
        g.state     = STATE_GAMEOVER;
        dealer_say(mode_line(g.mode->over_won));        /* dialogue over the GAME WON panel */
        printf("[Game] GAME WON after %d rounds\n", g.round);
    } else {
        g.round++;                /* advance to the next hand */
        clamp_bet();
        g.state = STATE_BET;
    }
    g.dirty = true;
}

/* Fresh game from the menu: reset both bankrolls and the round counter. When
 * `cursed` is true (PLAY hit during the menu's glitch) this is a Purgatory run
 * -- finite dealer pot and the shadow-dealer flavor. */
static void new_game(bool cursed) {
    g.mode           = cursed ? &MODE_PURGATORY : &MODE_NORMAL;
    set_world(cursed);               /* dark blood felt in Purgatory */
    g.dread          = 0;
    g.balance        = PLAYER_START;
    g.dealer_balance = DEALER_START;
    g.round          = 1;
    g.bet            = MIN_BET;
    clamp_bet();
    g.state = STATE_BET;
    g.dirty = true;
    if (g.mode->horror)
        dealer_say(mode_line(g.mode->intro));
}

/* ── Init / teardown ────────────────────────────────────────────────────── */
int game_init(void) {
    g.state          = STATE_MENU;
    g.deck_index     = 0;
    g.dealer_tick    = 0;
    g.outcome        = RESULT_PUSH;
    g.balance        = PLAYER_START;
    g.dealer_balance = DEALER_START;
    g.bet            = MIN_BET;
    g.round          = 0;
    g.result_flash   = 0;
    g.chip_anim      = 0;
    g.over_tick      = 0;
    g.game_won       = false;
    g.mode           = &MODE_NORMAL;
    g.dealer_show    = false;
    g.dealer_line    = "";
    g.dealer_chars   = 0;
    g.dealer_hold    = 0;
    g.snatch_anim    = 0;
    g.dealer_win_snatch = 0;
    g.chips_taken    = false;
    g.cursed_flash   = 0;
    g.dread          = 0;
    hand_reset(&g.player);
    hand_reset(&g.dealer);

    g.anim_settled_player = 0;
    g.anim_settled_dealer = 0;
    g.exit_requested = false;
    g.rules_page = 0;
    g.cursor_x = (uint16_t) (video_width()  / 2);
    g.cursor_y = (uint16_t) (video_height() / 2);
    g.prev_cursor_x = g.cursor_x;
    g.prev_cursor_y = g.cursor_y;
    g.dirty = true;                 /* first frame does a full flush */

    g.cursor      = sprite_load((xpm_map_t) cursor_xpm);
    g.card_face   = sprite_load((xpm_map_t) card_face_xpm);
    g.card_back   = sprite_load((xpm_map_t) card_back_xpm);
    g.chip        = sprite_load((xpm_map_t) chip_xpm);
    g.hand_fist   = sprite_load((xpm_map_t) hand_fist_xpm);
    g.hand_open   = sprite_load((xpm_map_t) hand_open_xpm);
    g.dealer_claw = sprite_load((xpm_map_t) dealer_claw_xpm);
    g.dealer_fist = sprite_load((xpm_map_t) dealer_fist_xpm);
    g.dealer_eyes = sprite_load((xpm_map_t) dealer_eyes_xpm);

    uint16_t pw = 120, ph = 40;
    uint16_t cx = (uint16_t) (video_width() / 2);
    uint16_t h_res = (uint16_t) video_width();

    /* Corner icon buttons (44x44). */
    g.rules    = button_create(8, 8, 44, 44, "",  0x1A3A6E, 0x2F5FAE);

    g.skin_index = 0;
    sprintf(g.skin_label, "SKIN: %s", render_skin_name(0));
    g.skin_btn = button_create((uint16_t)(h_res - 52), 8, 44, 44, g.skin_label, 0x3A2A6E, 0x5F4FAE);

    /* Centre menu buttons. */
    g.play  = button_create((uint16_t) ((video_width()  - pw) / 2), 390,
                            pw, ph, "PLAY", 0x1A6E1A, 0x2FAE2F);
    g.exit  = button_create((uint16_t) ((video_width()  - pw) / 2), 450,
                            pw, ph, "EXIT", 0x6E1A1A, 0xAE2F2F);
    if (g.play) g.play->anim_on_hover = true;
    if (g.exit) g.exit->anim_on_hover = true;

    g.hit   = button_create(300, 500,  90, 36, "HIT",   0x00BB00, 0x00FF00);
    g.stand = button_create(400, 500, 110, 36, "STAND", 0xCC0000, 0xFF2222);

    /* Betting controls, centred (positions mirrored by the bet panel render). */
    g.bet_minus = button_create((uint16_t)(cx - 170), 218, 48, 48, "-", 0x6E1A1A, 0xAE2F2F);
    g.bet_plus  = button_create((uint16_t)(cx + 122), 218, 48, 48, "+", 0x1A6E1A, 0x2FAE2F);
    g.allin     = button_create((uint16_t)(cx - 150), 292, 130, 38, "ALL IN", 0x6E5A1A, 0xB8860B);
    g.deal      = button_create((uint16_t)(cx + 20),  292, 130, 38, "DEAL", 0x1A6E1A, 0x2FAE2F);
    g.menu_btn  = button_create((uint16_t)(cx - 90),  340, 180, 30, "BACK TO MENU", 0x1A3A6E, 0x2F5FAE);

    if (g.play == NULL || g.rules == NULL || g.exit == NULL ||
        g.skin_btn == NULL || g.hit == NULL || g.stand == NULL ||
        g.bet_minus == NULL || g.bet_plus == NULL || g.allin == NULL ||
        g.deal == NULL || g.menu_btn == NULL)
        return 1;

    render_bake_table();    /* bake the felt table backdrop into the gradient */
    return 0;
}

void game_free(void) {
    sprite_free(g.cursor);
    sprite_free(g.card_face);
    sprite_free(g.card_back);
    sprite_free(g.chip);
    sprite_free(g.hand_fist);
    sprite_free(g.hand_open);
    sprite_free(g.dealer_claw);
    sprite_free(g.dealer_fist);
    sprite_free(g.dealer_eyes);
    button_free(g.play);
    button_free(g.rules);
    button_free(g.exit);
    button_free(g.skin_btn);
    button_free(g.hit);
    button_free(g.stand);
    button_free(g.bet_minus);
    button_free(g.bet_plus);
    button_free(g.allin);
    button_free(g.deal);
    button_free(g.menu_btn);
}

/* ── Timer-driven progression ───────────────────────────────────────────── */
void game_handle_timer(void) {
    /* Shadow-dealer dialogue: type the line out (face trembles while typing),
     * then linger and vanish. Runs in every state. */
    if (g.dealer_show) {
        int len = (int) strlen(g.dealer_line);
        if (g.dealer_chars < len * DEALER_TYPE_DIV) {
            g.dealer_chars++;
            g.dirty = true;                 /* tremble + new letter each tick */
        } else if (g.dealer_hold > 0) {
            if (--g.dealer_hold == 0) {
                g.dealer_show = false;
                g.dirty = true;
            }
        }
    }

    /* Wager chips sliding onto the table (runs in any table state). */
    if (g.chip_anim > 0) {
        g.chip_anim--;
        g.dirty = true;
    }

    /* Menu only: a rare, quick PURGATORY glitch flash. */
    if (g.state == STATE_MENU) {
        static int cursed_timer = 300;
        if (g.cursed_flash > 0) {
            g.cursed_flash--;
            g.dirty = true;
        } else if (--cursed_timer <= 0) {
            g.cursed_flash = CURSED_FLASH_LEN;
            cursed_timer   = 360 + (int)(entropy_events() % 480);   /* ~6..14s */
            g.dirty = true;
        }
        return;
    }

    /* Chip-delivery phase: once the dealer's hand has placed the chips and
     * retracted, deal the cards. */
    if (g.state == STATE_DEAL) {
        if (g.chip_anim == 0)
            start_hand();
        return;
    }

    /* WIN/LOSE banner + chip payout/snatch. Advance only once the banner has
     * flashed AND the chips have finished moving (payout slid in or claw left). */
    if (g.state == STATE_RESULT) {
        if (g.result_flash > 0) g.result_flash--;
        if (g.snatch_anim > 0) {
            g.snatch_anim--;
            if (g.snatch_anim == SNATCH_DURATION - 33)  /* the woosh moment */
                g.chips_taken = true;
        }
        if (g.dealer_win_snatch > 0) {
            g.dealer_win_snatch--;
            if (g.dealer_win_snatch <= 15)
                g.chips_taken = true;
        }
        /* Payout grab: chips vanish only once the hand retracts with both. */
        if (g.chip_anim > 0 && g.chip_anim <= 15)
            g.chips_taken = true;
        g.dirty = true;
        if (g.result_flash == 0 && g.chip_anim == 0 &&
            g.snatch_anim == 0 && g.dealer_win_snatch == 0)
            advance_after_result();
        return;
    }

    /* GAME OVER: auto-return to the menu. The loss sequence is shorter (and
     * animates every tick for the aggressive flash + growing eyes). */
    if (g.state == STATE_GAMEOVER) {
        int limit = g.game_won ? GAMEOVER_TICKS : 170;
        if (!g.game_won) g.dirty = true;
        if (++g.over_tick >= limit)
            go_menu();
        return;
    }

    /* Deal / draw slide animation. */
    if (g.anim_counter > 0) {
        g.anim_counter--;
        g.dirty = true;
        if (g.anim_counter == 0) {
            g.anim_settled_player = g.player.count;
            g.anim_settled_dealer = g.dealer.count;
            if (g.state == STATE_PLAYER) {
                if (hand_value(&g.player) > 21)
                    enter_result(RESULT_PLAYER_BUST);
                else if (hand_is_blackjack(&g.player) || hand_is_blackjack(&g.dealer))
                    enter_result(blackjack_settle(&g.player, &g.dealer));
            }
            if (g.state == STATE_DEALER)
                g.dealer_tick = DEALER_DELAY;
        }
        return;
    }

    if (g.state != STATE_DEALER)
        return;
    if (++g.dealer_tick < DEALER_DELAY)
        return;

    g.dealer_tick = 0;
    g.dirty = true;
    if (dealer_should_hit(&g.dealer)) {
        deal_to(&g.dealer);
        g.anim_settled_dealer = g.dealer.count - 1;
        g.anim_settled_player = g.player.count;
        g.anim_counter = ANIM_DURATION;
    } else {
        enter_result(blackjack_settle(&g.player, &g.dealer));
    }
}

/* ── Input ──────────────────────────────────────────────────────────────── */
void game_handle_mouse(int dx, int dy) {
    int nx = (int) g.cursor_x + dx * MOUSE_SENS;
    int ny = (int) g.cursor_y - dy * MOUSE_SENS;     /* mouse +y is up */

    if (nx < 0) nx = 0;
    if (nx >= (int) video_width())  nx = (int) video_width()  - 1;
    if (ny < 0) ny = 0;
    if (ny >= (int) video_height()) ny = (int) video_height() - 1;

    g.cursor_x = (uint16_t) nx;
    g.cursor_y = (uint16_t) ny;

    /* Only the buttons that belong to the current scene can be hovered, so gate
     * on g.state exactly like game_handle_click does -- no point hover-testing
     * HIT/STAND while we're in the menu. */
    bool hover_changed = false;
    switch (g.state) {
        case STATE_MENU:
            hover_changed |= button_update(g.play,     g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.rules,    g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.exit,     g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.skin_btn, g.cursor_x, g.cursor_y);
            break;

        case STATE_BET:
            hover_changed |= button_update(g.bet_minus, g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.bet_plus,  g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.allin,     g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.deal,      g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.menu_btn,  g.cursor_x, g.cursor_y);
            break;

        case STATE_PLAYER:
            if (g.anim_counter > 0) break;   /* mirror click: no hover mid-deal */
            hover_changed |= button_update(g.hit,   g.cursor_x, g.cursor_y);
            hover_changed |= button_update(g.stand, g.cursor_x, g.cursor_y);
            break;

        case STATE_RULES:
        case STATE_DEAL:
        case STATE_DEALER:
        case STATE_RESULT:
        case STATE_GAMEOVER:
            break;                           /* no buttons on these scenes */
    }
    if (hover_changed)
        g.dirty = true;
}

/* Shared actions so a mouse click and a key press do the same thing. */
static void action_deal(void) {
    if (!entropy_ready()) {
        printf("[Wait] need more entropy (events = %llu)\n",
               (unsigned long long) entropy_events());
        return;
    }
    clamp_bet();
    /* Reactive taunts: the shadow dealer reads your wager. */
    if (g.mode->horror) {
        if (g.bet >= g.balance)            dealer_say(mode_line(g.mode->allin));
        else if (g.balance <= MIN_BET * 3) dealer_say(mode_line(g.mode->low));
    }
    /* First the dealer's hand walks the chips in; cards are dealt once it
     * finishes (see STATE_DEAL handling in game_handle_timer). */
    hand_reset(&g.player);
    hand_reset(&g.dealer);
    g.chip_anim = CHIP_ANIM_DURATION;
    g.dealer_win_snatch = 0;
    g.snatch_anim       = 0;
    g.chips_taken       = false;
    g.state     = STATE_DEAL;
    g.dirty     = true;
}

static void action_hit(void) {
    deal_to(&g.player);
    g.dirty = true;
    g.anim_settled_player = g.player.count - 1;
    g.anim_settled_dealer = g.dealer.count;
    g.anim_counter = ANIM_DURATION;
}

static void action_stand(void) {
    g.anim_counter = 0;
    g.state = STATE_DEALER;
    g.dealer_tick = 0;
    g.dirty = true;
}

static void bet_inc(void) {
    if (g.balance < MIN_BET) return;        /* forced all-in, locked */
    g.bet += BET_STEP;
    clamp_bet();
    g.dirty = true;
}

static void bet_dec(void) {
    if (g.balance < MIN_BET) return;
    g.bet -= BET_STEP;
    clamp_bet();
    g.dirty = true;
}

static void bet_max(void) {
    g.bet = g.balance;
    clamp_bet();
    g.dirty = true;
}

/* Rules-page advance: page 1->2->menu */
static void rules_advance(void) {
    g.rules_page = (g.rules_page + 1) % 2;
    g.dirty = true;
    if (g.rules_page == 0)
        g.state = STATE_MENU;
}

void game_handle_click(void) {
    switch (g.state) {
        case STATE_MENU:
            if (button_contains(g.play, g.cursor_x, g.cursor_y))
                new_game(g.cursed_flash > 0);   /* glitch window -> Purgatory */
            else if (button_contains(g.rules, g.cursor_x, g.cursor_y)) {
                g.rules_page = 0;
                g.state = STATE_RULES;
                g.dirty = true;
            } else if (button_contains(g.exit, g.cursor_x, g.cursor_y)) {
                g.exit_requested = true;
                g.dirty = true;
            } else if (button_contains(g.skin_btn, g.cursor_x, g.cursor_y)) {
                apply_skin(g.skin_index + 1);
            }
            break;

        case STATE_RULES:
            rules_advance();
            break;

        case STATE_BET:
            if (button_contains(g.bet_minus, g.cursor_x, g.cursor_y))
                bet_dec();
            else if (button_contains(g.bet_plus, g.cursor_x, g.cursor_y))
                bet_inc();
            else if (button_contains(g.allin, g.cursor_x, g.cursor_y))
                bet_max();
            else if (button_contains(g.deal, g.cursor_x, g.cursor_y))
                action_deal();
            else if (button_contains(g.menu_btn, g.cursor_x, g.cursor_y))
                go_menu();
            break;

        case STATE_PLAYER:
            if (g.anim_counter > 0) break;
            if (button_contains(g.hit, g.cursor_x, g.cursor_y))
                action_hit();
            else if (button_contains(g.stand, g.cursor_x, g.cursor_y))
                action_stand();
            break;

        case STATE_GAMEOVER:
            go_menu();
            break;

        case STATE_DEAL:
        case STATE_RESULT:
        case STATE_DEALER:
            break;
    }
}

bool game_should_exit(void) {
    return g.exit_requested;
}

void game_handle_key(uint8_t scancode) {
    if (scancode & 0x80)              /* break/release code -> ignore */
        return;

    switch (g.state) {
        case STATE_MENU:
            if (scancode == KEY_SPACE || scancode == KEY_ENTER)
                new_game(g.cursed_flash > 0);   /* glitch window -> Purgatory */
            else if (scancode == KEY_T)
                apply_skin(g.skin_index + 1);
            break;

        case STATE_RULES:
            rules_advance();
            break;

        case STATE_BET:
            if (scancode == KEY_UP || scancode == KEY_KP_PLUS)
                bet_inc();
            else if (scancode == KEY_DOWN || scancode == KEY_KP_MINUS)
                bet_dec();
            else if (scancode == KEY_SPACE || scancode == KEY_ENTER)
                action_deal();
            break;

        case STATE_PLAYER:
            if (g.anim_counter > 0) break;
            if (scancode == KEY_SPACE)            /* SPACE = HIT */
                action_hit();
            else if (scancode == KEY_BACKSPACE)   /* BACKSPACE = STAND */
                action_stand();
            break;

        case STATE_GAMEOVER:
            go_menu();
            break;

        case STATE_DEAL:
        case STATE_RESULT:
        case STATE_DEALER:
            break;
    }
}

void game_render(void) {
    static unsigned frame = 0;
    frame++;
    text_set_anim((int)(frame / 3));    /* retro wobble advances ~20x/sec */

    render_scene(&g);               /* always redraw the back buffer (RAM, cheap) */

    /* Present by pushing only the scanlines that changed since last frame. The
     * cursor, the text wobble and every animation are all drawn into the back
     * buffer by render_scene, so the diff picks them up automatically -- and on
     * a static scene almost nothing is written to the (host-expensive) VRAM,
     * keeping the loop fast enough to drain the mouse without lag. */
    video_flush_diff();
    g.dirty = false;
}
