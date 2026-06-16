#ifndef RENDER_H
#define RENDER_H

#include "game.h"

#define TABLE_SKIN_COUNT 5

/* Skin selection (persists across bakes; call render_bake_table to apply). */
void        render_set_skin(int idx);
int         render_get_skin(void);
const char *render_skin_name(int idx);

/* Cursed world: when set, render_bake_table paints a dark blood-red felt
 * instead of the chosen skin (Purgatory mode). */
void        render_set_cursed(bool cursed);

/* Draws the full frame for the current game state into the back buffer. Does
 * NOT flush to VRAM -- the caller (game_render) decides how much to push, so it
 * can do a cheap dirty-rect flush when only the cursor moved. Reads state only. */
void render_scene(const Game *g);

/* Draw the static blackjack-table backdrop once and bake it into the gradient
 * buffer. Also calls video_gradient_precompute internally for the current skin,
 * so main.c does NOT need to call it separately. */
void render_bake_table(void);

#endif
