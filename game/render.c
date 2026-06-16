#include <stdio.h>
#include <string.h>

#include "render.h"
#include "video.h"
#include "text.h"
#include "blackjack.h"
#include "entropy.h"
#include "button.h"

/* ── Table skin definitions ─────────────────────────────────────────────── */
typedef struct { const char *name; uint32_t center, edge; } TableSkin;

static const TableSkin g_skins[TABLE_SKIN_COUNT] = {
    { "TEAL",     0x006666, 0x002222 },
    { "CLASSIC",  0x1A5C1A, 0x0A2A0A },
    { "CRIMSON",  0x5C1A1A, 0x2A0A0A },
    { "MIDNIGHT", 0x1A1A5C, 0x0A0A2A },
    { "OBSIDIAN", 0x222222, 0x080808 },
};
static int  g_skin_idx     = 0;
static bool g_cursed_world = false;   /* Purgatory -> blood-red felt */
static unsigned s_rframe   = 0;       /* frame counter for horror animations */

void render_set_skin(int idx) { g_skin_idx = ((idx % TABLE_SKIN_COUNT) + TABLE_SKIN_COUNT) % TABLE_SKIN_COUNT; }
int  render_get_skin(void)    { return g_skin_idx; }
const char *render_skin_name(int idx) { return g_skins[((idx % TABLE_SKIN_COUNT) + TABLE_SKIN_COUNT) % TABLE_SKIN_COUNT].name; }
void render_set_cursed(bool cursed) { g_cursed_world = cursed; }

static uint32_t color_lighten(uint32_t c, int a) {
    int r = (int)((c >> 16) & 0xFF) + a; if (r > 255) r = 255; if (r < 0) r = 0;
    int g = (int)((c >>  8) & 0xFF) + a; if (g > 255) g = 255; if (g < 0) g = 0;
    int b = (int)( c        & 0xFF) + a; if (b > 255) b = 255; if (b < 0) b = 0;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Forward declarations for helpers used before their definition. */
static int  isqrt_i(int v);
static void draw_circle_outline(int cx, int cy, int r, uint32_t c);
static void draw_filled_circle(int cx, int cy, int r, uint32_t c);

#define CARD_W      60
#define CARD_H      80
#define FAN_X_OFF   45
#define FAN_Y_OFF   (-5)
#define TABLE_RAIL_H 30

#define COL_GOLD    0xB8860B
#define COL_DIAMOND 0xFFD700
#define COL_WIN     0x00FF44
#define COL_LOSE    0xFF3333
#define COL_PUSH    0xFFD700

/* ── Corner icon buttons ─────────────────────────────────────────────────── */

/* Book icon for the RULES button (44x44). */
static void draw_book_icon(const Button *b) {
    if (b == NULL) return;
    int x = b->x, y = b->y;
    uint32_t bg    = b->hovered ? b->hover : b->base;
    uint32_t page  = 0xF0E8D0;   /* paper */
    uint32_t spine = 0x3A1800;   /* dark brown spine */
    uint32_t line  = 0xAAAAAA;   /* printed lines */

    video_rect((uint16_t)x, (uint16_t)y, 44, 44, bg);
    video_rect((uint16_t)x,      (uint16_t)y,      44, 1, 0xFFD700);
    video_rect((uint16_t)x,      (uint16_t)(y+43), 44, 1, 0xFFD700);
    video_rect((uint16_t)x,      (uint16_t)y,      1, 44, 0xFFD700);
    video_rect((uint16_t)(x+43), (uint16_t)y,      1, 44, 0xFFD700);

    /* Left page */
    video_rect((uint16_t)(x+5),  (uint16_t)(y+7),  15, 30, page);
    /* Right page */
    video_rect((uint16_t)(x+24), (uint16_t)(y+7),  15, 30, page);
    /* Spine */
    video_rect((uint16_t)(x+19), (uint16_t)(y+5),  6, 34, spine);

    /* Text lines on each page */
    for (int i = 0; i < 5; i++) {
        video_rect((uint16_t)(x+7),  (uint16_t)(y+12+i*5), 10, 1, line);
        video_rect((uint16_t)(x+26), (uint16_t)(y+12+i*5), 10, 1, line);
    }
}

/* Palette icon for the SKIN button: 5 coloured vertical stripes (44x44). */
static void draw_skin_icon(const Button *b, int skin_idx) {
    if (b == NULL) return;
    int x = b->x, y = b->y;
    uint32_t bg = b->hovered ? b->hover : b->base;

    static const uint32_t centers[TABLE_SKIN_COUNT] = {
        0x006666, 0x1A5C1A, 0x5C1A1A, 0x1A1A5C, 0x222222
    };

    video_rect((uint16_t)x, (uint16_t)y, 44, 44, bg);
    video_rect((uint16_t)x,      (uint16_t)y,      44, 1, 0xFFD700);
    video_rect((uint16_t)x,      (uint16_t)(y+43), 44, 1, 0xFFD700);
    video_rect((uint16_t)x,      (uint16_t)y,      1, 44, 0xFFD700);
    video_rect((uint16_t)(x+43), (uint16_t)y,      1, 44, 0xFFD700);

    /* 5 stripes of width 6, 1px gap, centred vertically */
    for (int i = 0; i < TABLE_SKIN_COUNT; i++) {
        int sx = x + 4 + i * 8;
        int sy = y + 8;
        int sh = 28;
        video_rect((uint16_t)sx, (uint16_t)sy, 6, sh, centers[i]);
        if (i == skin_idx) {
            /* white highlight border around current skin */
            video_rect((uint16_t)(sx-1), (uint16_t)(sy-1), 8, 1, 0xFFFFFF);
            video_rect((uint16_t)(sx-1), (uint16_t)(sy+sh), 8, 1, 0xFFFFFF);
            video_rect((uint16_t)(sx-1), (uint16_t)(sy-1), 1, sh+2, 0xFFFFFF);
            video_rect((uint16_t)(sx+6), (uint16_t)(sy-1), 1, sh+2, 0xFFFFFF);
        }
    }

    /* Small 'T' hint below the stripes */
    text_draw("T", (uint16_t)(x + 18), (uint16_t)(y + 38), 0xAAAAAA);
}

/* ── Card suit pixel-art (fixed ~10px size) ─────────────────────────────── */
static void draw_suit_diamond(int cx, int cy, uint32_t c) {
    video_rect((uint16_t)(cx),   (uint16_t)(cy-5), 1, 1, c);
    video_rect((uint16_t)(cx-1), (uint16_t)(cy-4), 3, 1, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy-3), 5, 1, c);
    video_rect((uint16_t)(cx-3), (uint16_t)(cy-2), 7, 1, c);
    video_rect((uint16_t)(cx-4), (uint16_t)(cy-1), 9, 1, c);
    video_rect((uint16_t)(cx-4), (uint16_t)(cy),   9, 1, c);
    video_rect((uint16_t)(cx-3), (uint16_t)(cy+1), 7, 1, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy+2), 5, 1, c);
    video_rect((uint16_t)(cx-1), (uint16_t)(cy+3), 3, 1, c);
    video_rect((uint16_t)(cx),   (uint16_t)(cy+4), 1, 1, c);
}

static void draw_suit_heart(int cx, int cy, uint32_t c) {
    video_rect((uint16_t)(cx-3), (uint16_t)(cy-4), 3, 1, c);
    video_rect((uint16_t)(cx+1), (uint16_t)(cy-4), 3, 1, c);
    video_rect((uint16_t)(cx-4), (uint16_t)(cy-3), 4, 2, c);
    video_rect((uint16_t)(cx+1), (uint16_t)(cy-3), 4, 2, c);
    video_rect((uint16_t)(cx-4), (uint16_t)(cy-1), 9, 1, c);
    video_rect((uint16_t)(cx-3), (uint16_t)(cy),   7, 1, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy+1), 5, 1, c);
    video_rect((uint16_t)(cx-1), (uint16_t)(cy+2), 3, 1, c);
    video_rect((uint16_t)(cx),   (uint16_t)(cy+3), 1, 1, c);
}

static void draw_suit_spade(int cx, int cy, uint32_t c) {
    video_rect((uint16_t)(cx),   (uint16_t)(cy-5), 1, 1, c);
    video_rect((uint16_t)(cx-1), (uint16_t)(cy-4), 3, 1, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy-3), 5, 1, c);
    video_rect((uint16_t)(cx-3), (uint16_t)(cy-2), 7, 1, c);
    video_rect((uint16_t)(cx-4), (uint16_t)(cy-1), 9, 1, c);
    video_rect((uint16_t)(cx-3), (uint16_t)(cy),   7, 1, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy+1), 2, 2, c);
    video_rect((uint16_t)(cx+1), (uint16_t)(cy+1), 2, 2, c);
    video_rect((uint16_t)(cx-1), (uint16_t)(cy+2), 3, 1, c);
    video_rect((uint16_t)(cx),   (uint16_t)(cy+3), 1, 1, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy+4), 5, 1, c);
}

static void draw_suit_club(int cx, int cy, uint32_t c) {
    /* top circle */
    video_rect((uint16_t)(cx-2), (uint16_t)(cy-5), 5, 1, c);
    video_rect((uint16_t)(cx-3), (uint16_t)(cy-4), 7, 2, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy-2), 5, 1, c);
    /* left circle */
    video_rect((uint16_t)(cx-6), (uint16_t)(cy-2), 4, 1, c);
    video_rect((uint16_t)(cx-7), (uint16_t)(cy-1), 6, 2, c);
    video_rect((uint16_t)(cx-6), (uint16_t)(cy+1), 4, 1, c);
    /* right circle */
    video_rect((uint16_t)(cx+3), (uint16_t)(cy-2), 4, 1, c);
    video_rect((uint16_t)(cx+2), (uint16_t)(cy-1), 6, 2, c);
    video_rect((uint16_t)(cx+3), (uint16_t)(cy+1), 4, 1, c);
    /* centre fill + stem + foot */
    video_rect((uint16_t)(cx-2), (uint16_t)(cy-3), 5, 5, c);
    video_rect((uint16_t)(cx),   (uint16_t)(cy+2), 1, 2, c);
    video_rect((uint16_t)(cx-2), (uint16_t)(cy+4), 5, 1, c);
}

/* ── Card back embellishment (diamond pattern) ──────────────────────────── */
static void draw_card_back_detail(uint16_t x, uint16_t y) {
    uint32_t c = 0x4444FF;
    for (int i = 0; i < 14; i++) {
        int half = (i < 7) ? i : (13 - i);
        int w = half * 2 + 4;
        video_rect(x + 30 - w / 2, (uint16_t)(y + 19 + i * 3), (uint16_t) w, 3, c);
    }
}

/* ── Fanned hand with optional deal animation ──────────────────────────── */
static void draw_fanned_hand(const Hand *h, uint16_t center_x, uint16_t start_y,
                             bool hide_first, Sprite *face_spr, Sprite *back_spr,
                             float progress, uint16_t deck_x, uint16_t deck_y,
                             uint8_t anim_settled) {
    int n = h->count > 0 ? h->count : 1;
    unsigned total_w = (n - 1) * FAN_X_OFF + CARD_W;
    uint16_t start_x = center_x - total_w / 2;
    for (uint8_t i = 0; i < h->count && i < HAND_MAX; i++) {
        uint16_t tx = (uint16_t)(start_x + i * FAN_X_OFF);
        uint16_t ty = (uint16_t)(start_y + i * FAN_Y_OFF);
        uint16_t cx, cy;
        if (i < anim_settled || progress >= 1.0f) {
            cx = tx;
            cy = ty;
        } else {
            cx = (uint16_t)(deck_x + (int)((int)tx - (int)deck_x) * progress);
            cy = (uint16_t)(deck_y + (int)((int)ty - (int)deck_y) * progress);
        }
        if (hide_first && i == 0) {
            if (back_spr != NULL) {
                sprite_draw(back_spr, cx, cy);
                draw_card_back_detail(cx, cy);
            } else {
                video_rounded_rect(cx, cy, CARD_W, CARD_H, 4, 0x000080, 0xFFFFFF);
            }
        } else {
            if (face_spr != NULL)
                sprite_draw(face_spr, cx, cy);
            text_card(h->cards[i], cx, cy);
        }
    }
}

/* ── Score bubble ──────────────────────────────────────────────────────── */
static void draw_score_bubble(const Hand *h, uint16_t cx, uint16_t cy,
                              uint32_t text_color, bool hide_first) {
    char buf[4];
    /* While a hole card is face-down (player's turn), the bubble must reflect
     * only the up-cards -- revealing the dealer's full total would leak the
     * hidden card. Tally the visible cards (everything but index 0) through the
     * same hand_value logic so ace 11/1 handling stays correct. */
    Hand visible;
    const Hand *src = h;
    if (hide_first && h->count > 0) {
        hand_reset(&visible);
        for (uint8_t i = 1; i < h->count && i < HAND_MAX; i++)
            hand_add(&visible, h->cards[i]);
        src = &visible;
    }
    int val = hand_value(src);
    sprintf(buf, "%d", val);
    unsigned tw = text_width(buf, 2);
    uint16_t bw = tw + 10;
    uint16_t bx = cx - bw / 2;
    video_rounded_rect(bx, cy, bw, 24, 4, 0x222222, 0x888888);
    text_draw(buf, bx + 5, (uint16_t)(cy + 5), text_color);
}

/* ── Filled circle helper (uses isqrt_i already defined above) ───────────── */
static void draw_filled_circle(int cx, int cy, int r, uint32_t c) {
    int sw = (int)video_width(), sh = (int)video_height();
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= sh) continue;
        int dx = isqrt_i(r * r - dy * dy);
        int x0 = cx - dx; if (x0 < 0) x0 = 0;
        int x1 = cx + dx; if (x1 >= sw) x1 = sw - 1;
        if (x1 < x0) continue;
        video_rect((uint16_t)x0, (uint16_t)y, (uint16_t)(x1 - x0 + 1), 1, c);
    }
}

/* ── A single casino chip (radius ~15) ───────────────────────────────────── */
static void draw_chip(uint16_t cx, uint16_t cy, uint32_t fill) {
    int x = (int)cx, y = (int)cy;

    /* drop shadow */
    draw_filled_circle(x + 2, y + 3, 14, 0x111111);

    /* outer dark border */
    draw_filled_circle(x, y, 15, 0x1A1A1A);

    /* white band */
    draw_filled_circle(x, y, 13, 0xDDDDDD);

    /* 8 coloured inlay segments on the white band */
    static const int sdx[8] = { 11,  8,  0, -8, -11, -8,  0,  8 };
    static const int sdy[8] = {  0,  8, 11,  8,   0, -8,-11, -8 };
    for (int i = 0; i < 8; i++) {
        int bx = x + sdx[i] - 2;
        int by = y + sdy[i] - 2;
        if (bx >= 0 && by >= 0)
            video_rect((uint16_t)bx, (uint16_t)by, 4, 4, fill);
    }

    /* main body */
    draw_filled_circle(x, y, 10, fill);

    /* inner white ring */
    draw_circle_outline(x, y, 6, 0xDDDDDD);
    draw_circle_outline(x, y, 7, 0xDDDDDD);

    /* highlight (top-left glint) */
    draw_filled_circle(x - 3, y - 4, 3, color_lighten(fill, 55));
}

/* ── Decorative chip stacks for the menu ─────────────────────────────────── */
static void draw_menu_chips(uint16_t h_res, uint16_t v_res) {
    static const uint32_t cols[5] = {
        0x1A8A1A,   /* green  */
        0xBB1A1A,   /* red    */
        0x1A1ABB,   /* blue   */
        0xBBBBBB,   /* white  */
        0x111122,   /* black  */
    };
    int rail_y = (int)v_res - TABLE_RAIL_H - 22;

    /* left stack: 5 chips rising to the left */
    for (int i = 4; i >= 0; i--)
        draw_chip((uint16_t)(95 + i * 2), (uint16_t)(rail_y - i * 7),
                  cols[i]);

    /* right stack */
    for (int i = 4; i >= 0; i--)
        draw_chip((uint16_t)(h_res - 95 - i * 2), (uint16_t)(rail_y - i * 7),
                  cols[(i + 2) % 5]);

    /* small scattered chips near centre-bottom */
    draw_chip((uint16_t)(h_res / 2 - 40), (uint16_t)(rail_y),      cols[1]);
    draw_chip((uint16_t)(h_res / 2 + 40), (uint16_t)(rail_y),      cols[0]);
    draw_chip((uint16_t)(h_res / 2),      (uint16_t)(rail_y - 10), cols[3]);
}

/* ── Draw the physical deck stack (top-left table prop) ─────────────────── */
static void draw_deck(uint16_t x, uint16_t y, const Sprite *back_spr) {
    if (back_spr == NULL) return;
    for (int i = 2; i >= 0; i--) {
        uint16_t dx = (uint16_t)(x + i * 2);
        uint16_t dy = (uint16_t)(y + i * 2);
        sprite_draw(back_spr, dx, dy);
        draw_card_back_detail(dx, dy);
    }
}

/* ── Casino table decorations ───────────────────────────────────────────── */
static void draw_table_decor(uint16_t h_res, uint16_t v_res, uint16_t rail_y) {
    uint32_t gold = COL_GOLD;
    uint16_t br = 10;
    (void) v_res;

    /* Gold felt border — closed rectangle */
    video_rect(br, br, h_res - br * 2, 2, gold);
    video_rect(br, rail_y - 2, h_res - br * 2, 2, gold);
    video_rect(br, br, 2, rail_y - br - 2, gold);
    video_rect(h_res - br - 2, br, 2, rail_y - br - 2, gold);

    /* Rail pattern — wood-grain horizontal lines + vertical panel divisions */
    for (int i = 0; i < 6; i++) {
        uint16_t ly = (uint16_t)(rail_y + 2 + i * 5);
        video_rect(0, ly, h_res, 1, 0x3A2A1A);
    }
    for (int p = 1; p < 5; p++) {
        uint16_t px = (uint16_t)(h_res / 5 * p);
        video_rect(px, rail_y, 2, 30, 0x1A0A00);
    }
}

/* ── Blackjack-table backdrop (baked once into the gradient buffer) ───────── */
static int isqrt_i(int v) {
    if (v < 0) return 0;
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

/* Lower half of a circle outline (a downward "smile" arc) via midpoint algo. */
static void draw_lower_arc(int cx, int cy, int r, uint32_t c) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        video_pixel((uint16_t)(cx + x), (uint16_t)(cy + y), c);
        video_pixel((uint16_t)(cx - x), (uint16_t)(cy + y), c);
        video_pixel((uint16_t)(cx + y), (uint16_t)(cy + x), c);
        video_pixel((uint16_t)(cx - y), (uint16_t)(cy + x), c);
        x++;
        if (d < 0) d += 2 * x + 1;
        else { y--; d += 2 * (x - y) + 1; }
    }
}

static void draw_circle_outline(int cx, int cy, int r, uint32_t c) {
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        video_pixel((uint16_t)(cx + x), (uint16_t)(cy + y), c);
        video_pixel((uint16_t)(cx - x), (uint16_t)(cy + y), c);
        video_pixel((uint16_t)(cx + x), (uint16_t)(cy - y), c);
        video_pixel((uint16_t)(cx - x), (uint16_t)(cy - y), c);
        video_pixel((uint16_t)(cx + y), (uint16_t)(cy + x), c);
        video_pixel((uint16_t)(cx - y), (uint16_t)(cy + x), c);
        video_pixel((uint16_t)(cx + y), (uint16_t)(cy - x), c);
        video_pixel((uint16_t)(cx - y), (uint16_t)(cy - x), c);
        x++;
        if (d < 0) d += 2 * x + 1;
        else { y--; d += 2 * (x - y) + 1; }
    }
}

void render_bake_table(void) {
    uint16_t h_res = (uint16_t) video_width();
    uint16_t v_res = (uint16_t) video_height();
    uint16_t rail_y = v_res - TABLE_RAIL_H;

    /* Cursed (Purgatory) world uses a dark blood-red felt; otherwise the skin. */
    uint32_t center = g_cursed_world ? 0x1A0000 : g_skins[g_skin_idx].center;
    uint32_t edge   = g_cursed_world ? 0x040000 : g_skins[g_skin_idx].edge;
    uint32_t arc1   = g_cursed_world ? 0x7A0000 : COL_GOLD;
    uint32_t arc2   = g_cursed_world ? 0x3A0000 : 0x2E8C8C;

    video_gradient_precompute(center, edge);
    video_gradient_apply();

    /* Felt weave texture: subtle diagonal line pattern. */
    uint32_t tex = color_lighten(center, 18);
    for (unsigned ty = 0; ty < (unsigned)v_res; ty++)
        for (unsigned tx = 0; tx < (unsigned)h_res; tx++)
            if ((tx + ty) % 10 == 0)
                video_pixel((uint16_t)tx, (uint16_t)ty, tex);

    /* Curved table edge + inner betting line, centred on the dealer (top). */
    int cx = h_res / 2, cy = 40;
    for (int t = 0; t < 3; t++) draw_lower_arc(cx, cy, 470 - t, arc1);
    for (int t = 0; t < 2; t++) draw_lower_arc(cx, cy, 400 - t, arc2);

    /* Betting spots along the inner arc. */
    static const int spot_dx[5] = { -300, -150, 0, 150, 300 };
    for (int i = 0; i < 5; i++) {
        int dx = spot_dx[i];
        int yy = cy + isqrt_i(400 * 400 - dx * dx);
        draw_circle_outline(cx + dx, yy, 17, arc1);
        draw_circle_outline(cx + dx, yy, 16, arc1);
    }

    /* Wooden rail + gold felt border. */
    video_rect(0, rail_y, h_res, TABLE_RAIL_H, 0x2A1A0A);
    video_rect(0, rail_y - 2, h_res, 2, 0x1A0A00);
    draw_table_decor(h_res, v_res, rail_y);

    video_gradient_capture();          /* snapshot back -> gradient buffer */
}

/* ── Centre casino emblem (only on the menu / empty table) ──────────────── */
static void draw_emblem(uint16_t h_res) {
    uint32_t gold = COL_GOLD;
    uint32_t diam = COL_DIAMOND;
    uint16_t cx = h_res / 2;
    uint16_t cy = (85 + 110 + 355) / 2;

    video_rect(cx - 140, 197, 280, 1, gold);
    video_rect(cx - 140, 201, 280, 1, gold);

    const char *ent = "ENTROPY";
    unsigned ew = text_width(ent, 2);
    text_draw(ent, cx - ew / 2, 207, COL_DIAMOND);
    video_rect(cx - ew / 2 - 8, 207, 2, 14, gold);
    video_rect(cx + ew / 2 + 6, 207, 2, 14, gold);

    video_rect(cx - 100, 226, 200, 1, gold);

    for (int i = 0; i < 25; i++) {
        int half = (i < 13) ? i : (24 - i);
        int w = half * 9 + 12;
        video_rect(cx - w / 2, (uint16_t)(cy - 48 + i * 4), (uint16_t)w, 2, diam);
    }
    for (int i = 0; i < 17; i++) {
        int half = (i < 9) ? i : (16 - i);
        int w = half * 6 + 8;
        video_rect(cx - w / 2, (uint16_t)(cy - 32 + i * 4), (uint16_t)w, 2, 0x44AAAA);
    }
    video_rect(cx - 80, (uint16_t)(cy - 2), 20, 2, gold);
    video_rect(cx + 60, (uint16_t)(cy - 2), 20, 2, gold);
    video_rect(cx - 100, (uint16_t)(cy + 50), 200, 1, gold);

    const char *cas = "CASINO";
    unsigned cw = text_width(cas, 2);
    text_draw(cas, cx - cw / 2, (uint16_t)(cy + 56), COL_DIAMOND);
    video_rect(cx - cw / 2 - 8, (uint16_t)(cy + 56), 2, 14, gold);
    video_rect(cx + cw / 2 + 6, (uint16_t)(cy + 56), 2, 14, gold);
    video_rect(cx - 140, (uint16_t)(cy + 78), 280, 1, gold);

    /* Card suits flanking the diamond emblem. */
    draw_suit_spade(  (int)cx - 112, cy - 22, 0xDDDDFF);
    draw_suit_club(   (int)cx - 112, cy + 28, 0xDDFFDD);
    draw_suit_heart(  (int)cx + 112, cy - 22, 0xFF5555);
    draw_suit_diamond((int)cx + 112, cy + 28, 0xFF5555);
}

/* ── HUD: dealer purse (top-left) + player purse / round / bet (bottom-left) */
static void draw_dealer_label(const Game *g) {
    char buf[20];
    video_rounded_rect(36, 132, 188, 40, 6, 0x331111, 0xAE2F2F);
    /* Purgatory: a finite, drainable pot. Normal: the house is infinite, so it
     * shows a name rather than a number. */
    if (g->mode->finite_house)
        sprintf(buf, "DEALER$ %d", g->dealer_balance);
    else
        sprintf(buf, "HOUSE");
    text_draw(buf, 48, 140, 0xFF6655);
}

static void draw_player_box(const Game *g) {
    char buf[24];
    bool horror = g->mode->horror;

    video_rounded_rect(36, 416, 188, 132, 8,
                       horror ? 0x1A0000 : 0x003333,
                       horror ? 0x7A0000 : COL_GOLD);

    sprintf(buf, "ROUND %d", g->round);
    text_draw(buf, 48, 426, horror ? 0xAA4444 : COL_DIAMOND);

    if (horror) {
        /* The wallet bleeds: trembles, drips red, and flickers to SOUL. */
        int tdx = (int)((s_rframe * 7u) % 3u) - 1;
        int tdy = (int)((s_rframe * 5u) % 3u) - 1;
        bool soul = ((s_rframe / 36) % 7) == 0;     /* occasional flicker */
        if (soul) sprintf(buf, "SOUL %d", g->balance);
        else      sprintf(buf, "WALLET$ %d", g->balance);
        text_draw(buf, (uint16_t)(48 + tdx), (uint16_t)(470 + tdy), 0xDD0000);

        unsigned seed = s_rframe * 2654435761u + 55u;
        for (int i = 0; i < 6; i++) {              /* dripping blood streaks */
            seed = seed * 1103515245u + 12345u;
            int dx   = 50 + (int)((seed >> 16) % 150);
            int dlen = 3 + (int)((seed >> 9) % 8) + (int)((s_rframe / 4 + i * 5) % 10);
            video_rect((uint16_t) dx, 486, 1, (uint16_t) dlen, 0x880000);
        }
    } else {
        sprintf(buf, "WALLET$ %d", g->balance);
        text_draw(buf, 48, 470, 0x33DD33);
    }

    sprintf(buf, "BET$ %d", g->bet);
    text_draw(buf, 48, 514, horror ? 0xCC2222 : 0xFFFFFF);

    draw_chip(196, 480, 0xCC0000);
}

/* ── Wager chips: a denomination stack that slides up from below on a deal ── */
static void draw_wager_chips(uint16_t x, int base_y, int bet) {
    static const int      denom[3] = { 25, 10, 5 };
    static const uint32_t col[3]   = { 0x202033, 0x0030CC, 0xCC2020 };  /* black/blue/red */
    int n = 0, rem = bet;
    for (int d = 0; d < 3; d++)
        while (rem >= denom[d] && n < 9) {
            draw_chip(x, (uint16_t)(base_y - n * 7), col[d]);
            rem -= denom[d];
            n++;
        }
    if (n == 0)                 /* bet below 5 (forced all-in) -> a single chip */
        draw_chip(x, (uint16_t) base_y, 0xCC2020);
}

/* ── Dealer's hand+arm sprite (from Arm.png) ─────────────────────────────── */
static void draw_hand_ex(uint16_t cx, int ty, const Sprite *spr,
                         bool flip_h, bool flip_v,
                         uint32_t tint_dark, uint32_t tint_light,
                         uint8_t threshold) {
    if (spr == NULL) return;
    int x = (int) cx - (int) spr->width / 2;
    if (x < 0) x = 0;
    sprite_draw_ex(spr, (uint16_t) x, ty, flip_h, flip_v,
                   tint_dark, tint_light, threshold);
}

/* Nearest-neighbour scaled sprite blit, centred at (cx,cy). scale = num/den. */
static void draw_sprite_scaled(const Sprite *sp, int cx, int cy, int num, int den) {
    if (sp == NULL || num <= 0 || den <= 0) return;
    int dw = (int) sp->width  * num / den;
    int dh = (int) sp->height * num / den;
    if (dw <= 0 || dh <= 0) return;
    int ox = cx - dw / 2, oy = cy - dh / 2;
    int sw = (int) video_width(), sh = (int) video_height();
    for (int j = 0; j < dh; j++) {
        int yy = oy + j;
        if (yy < 0 || yy >= sh) continue;
        int sy = j * den / num;
        if (sy >= (int) sp->height) sy = (int) sp->height - 1;
        const uint32_t *srow = sp->pixmap + sy * sp->width;
        for (int i = 0; i < dw; i++) {
            int xx = ox + i;
            if (xx < 0 || xx >= sw) continue;
            int sx = i * den / num;
            if (sx >= (int) sp->width) sx = (int) sp->width - 1;
            uint32_t c = srow[sx];
            if (c != sp->transparent)
                video_pixel((uint16_t) xx, (uint16_t) yy, c);
        }
    }
}

/* One side of the wager delivery.
 *   from_top  — true = arm descends from above, false = rises from below
 *   chip_cx   — own chip stack x
 *   chip_cx2  — opponent chip stack x (used in collect to grab both; set equal
 *               to chip_cx when only own chips are taken)
 *   flip_h    — mirror the arm sprite horizontally
 *   flip_v    — mirror the arm sprite vertically (so shoulder enters first
 *               when descending from above)
 *   tint_dark, tint_light, threshold — passed to draw_hand_ex (0xFFFFFF = skip)
 */
static void draw_side_delivery(int chip_anim, bool collect,
                               uint16_t chip_cx, uint16_t chip_cx2,
                               bool flip_h, bool flip_v, bool from_top,
                               uint32_t tint_dark, uint32_t tint_light,
                               uint8_t threshold,
                               const Sprite *fist, const Sprite *open,
                               uint16_t v_res, int bet) {
    if (chip_anim <= 0) return;
    const int total = CHIP_ANIM_DURATION;  /* 100 */
    const int ty_spot = 300;
    bool has_two = (collect && chip_cx2 != chip_cx);

    /* The hand is at offset hand_ofs from the sprite top (0 = top of sprite,
     * spr_h = bottom). With flip_v=true the sprite is flipped so the hand
     * is at the bottom. */
    int spr_h = 380;  /* hand_fist/hand_open is 380 px tall */
    int hand_ofs = flip_v ? spr_h : 0;

    /* Start and end positions for the sprite top (ty) so that the HAND
     * always travels the same distance (v_res - ty_spot) and ends at ty_spot.
     * from_top=false: hand rises from below the screen.
     * from_top=true:  hand descends from above the screen. */
    int ty_start, ty_end;
    if (from_top) {
        ty_end   = ty_spot - hand_ofs;
        int dist = (int)v_res - ty_spot;
        ty_start = (ty_spot - dist) - hand_ofs;
    } else {
        ty_start = (int)v_res - hand_ofs;
        ty_end   = ty_spot - hand_ofs;
    }

    int ty;
    if (chip_anim > 60) {
        int num = total - chip_anim;      /* 0..40 */
        ty = ty_start + (ty_end - ty_start) * num / (total - 60);
    } else if (chip_anim > 40) {
        ty = ty_end;
    } else {
        int num = 40 - chip_anim;         /* 0..40 */
        ty = ty_end + (ty_start - ty_end) * num / 40;
    }

    /* Determine horizontal position and open/close state. */
    int arm_cx;
    bool open_hand;

    if (has_two) {
        /* Two-target collect: own chips → opponent's chips */
        if (chip_anim > 70) {
            arm_cx     = (int) chip_cx;
            open_hand  = true;        /* extending toward own chips */
        } else if (chip_anim > 60) {
            arm_cx     = (int) chip_cx;
            open_hand  = false;       /* grab own chips */
        } else if (chip_anim > 30) {
            /* slide toward opponent's chips */
            int num = chip_anim - 30; /* 30..0 */
            int range = 30;
            arm_cx = (int) chip_cx + ((int) chip_cx2 - (int) chip_cx) * (range - num) / range;
            open_hand = (chip_anim > 40); /* open while sliding, close at arrival */
        } else if (chip_anim > 15) {
            arm_cx     = (int) chip_cx2;
            open_hand  = false;       /* grab opponent's chips */
        } else {
            arm_cx     = (int) chip_cx2;
            open_hand  = false;       /* retract */
        }
    } else {
        /* Single-target: own chips only */
        arm_cx = (int) chip_cx;
        if (chip_anim > 60) {
            open_hand = collect;      /* extending: open if collect, fist if place */
        } else if (chip_anim > 40) {
            open_hand = !collect;     /* hold: open if place, closed if collect */
        } else {
            open_hand = false;        /* retracting: fist */
        }
    }

    /* Which chip x to show when hand is open. */
    int show_cx;
    if (has_two) {
        if (chip_anim > 70)      show_cx = (int) chip_cx;
        else if (chip_anim > 60) show_cx = (int) chip_cx;   /* closed, no chips shown */
        else if (chip_anim > 30) show_cx = arm_cx;          /* chips follow sliding hand */
        else                     show_cx = (int) chip_cx2;
    } else {
        show_cx = (int) chip_cx;
    }

    draw_hand_ex((uint16_t) arm_cx, ty, open_hand ? open : fist,
                 flip_h, flip_v, tint_dark, tint_light, threshold);
    /* Show chips in the open palm only when placing or receiving — skip
     * during the two-target slide (hand extends toward the second stack
     * but is still empty). */
    bool sliding_palm = (has_two && chip_anim <= 60 && chip_anim > 30);
    if (open_hand && !sliding_palm)
        draw_wager_chips((uint16_t) show_cx, 300, bet);
}

/* Player's hand (left side, rises from below) for deal/collect. */
static void draw_player_delivery(const Game *g, uint16_t cx, uint16_t v_res) {
    bool collect = (g->state == STATE_RESULT);
    /* On win, player grabs own left chips then dealer's right chips. */
    uint16_t own = (uint16_t)((int)cx - 55);
    uint16_t opp = (uint16_t)((int)cx + 55);
    draw_side_delivery(g->chip_anim, collect, own, collect ? opp : own,
                       false, false, false, 0xFFFFFF, 0xFFFFFF, 0,
                       g->hand_fist, g->hand_open, v_res, g->bet);
}

/* Dealer's mirrored arm (right side, descends from top, red suit + black skin). */
static void draw_dealer_delivery(const Game *g, uint16_t cx, uint16_t v_res) {
    bool collect = (g->state == STATE_RESULT);
    /* On win, dealer grabs own right chips then player's left chips. */
    uint16_t own = (uint16_t)((int)cx + 55);
    uint16_t opp = (uint16_t)((int)cx - 55);
    draw_side_delivery(g->chip_anim, collect, own, collect ? opp : own,
                       true, true, true, 0xFF4444, 0x554433, 85,
                       g->hand_fist, g->hand_open, v_res, g->bet);
}

/* Dealer's mirrored arm snatching opponent's chips when dealer wins. */
static void draw_dealer_win_collect(const Game *g, uint16_t cx, uint16_t v_res) {
    int sa = g->dealer_win_snatch;
    if (sa <= 0) return;
    uint16_t own = (uint16_t)((int)cx + 55);
    uint16_t opp = (uint16_t)((int)cx - 55);
    draw_side_delivery(sa, true, own, opp,
                       true, true, true, 0xFF4444, 0x554433, 85,
                       g->hand_fist, g->hand_open, v_res, g->bet);
}

/* ── Demon claw dropping chips during DEAL (purgatory mode) ───────────────── */
static void draw_dealer_drop(const Game *g, uint16_t cx, uint16_t v_res) {
    int chip_anim = g->chip_anim;
    if (chip_anim <= 0) return;
    (void)v_res;
    uint16_t chip_cx = (uint16_t)((int)cx + 55);
    const int ty_spot = 300;
    int sw = g->dealer_claw ? (int)g->dealer_claw->width : 120;
    int sh = g->dealer_claw ? (int)g->dealer_claw->height : 180;

    if (chip_anim > 60) {
        /* Descend: open claw drops in from upper-right with chips visible. */
        int num = CHIP_ANIM_DURATION - chip_anim;
        int x = ((int)cx + 70) + ((int)chip_cx - sw / 2 - ((int)cx + 70)) * num / 40;
        int y = (-sh) + (ty_spot - sh + 18 - (-sh)) * num / 40;
        if (g->dealer_claw && x >= 0 && y > -sh) {
            int dx = x < 0 ? 0 : x;
            int dy = y < 0 ? 0 : y;
            sprite_draw(g->dealer_claw, (uint16_t)dx, (uint16_t)dy);
            draw_wager_chips((uint16_t)(dx + sw / 2), (uint16_t)(dy + sh - 12), g->bet);
        }
    } else if (chip_anim > 40) {
        /* Hold at position: claw open, chips on table (static). */
        int x = (int)chip_cx - sw / 2;
        int y = ty_spot - sh + 18;
        if (g->dealer_claw && x >= 0)
            sprite_draw(g->dealer_claw, (uint16_t)x, (uint16_t)(y < 0 ? 0 : y));
    } else {
        /* Retract: closed fist withdraws up-right, empty. */
        int fw = g->dealer_fist ? (int)g->dealer_fist->width : 100;
        int fh = g->dealer_fist ? (int)g->dealer_fist->height : 100;
        int num = 40 - chip_anim;
        int x = ((int)chip_cx - fw / 2) + (((int)cx + 50) - ((int)chip_cx - fw / 2)) * num / 40;
        int y = (ty_spot - fh + 30) + (-fh - (ty_spot - fh + 30)) * num / 40;
        if (g->dealer_fist && x >= 0 && y > -fh)
            sprite_draw(g->dealer_fist, (uint16_t)(x < 0 ? 0 : x), (uint16_t)(y < 0 ? 0 : y));
    }
}

/* ── Shadow-dealer claw snatching the chips (on a loss) ──────────────────── */
static void draw_woosh(int cx, int cy) {
    for (int i = 0; i < 5; i++) {
        int yy = cy - 16 + i * 8;
        int len = 56 - (i & 1) * 18;
        video_rect((uint16_t)(cx - len / 2), (uint16_t) yy, (uint16_t) len, 2,
                   (i & 1) ? 0xBBBBBB : 0xFFFFFF);
    }
    for (int i = 0; i < 10; i++) {       /* diagonal speed flecks */
        video_pixel((uint16_t)(cx + 18 + i * 3), (uint16_t)(cy - 14 + i * 3), 0xFFFFFF);
        video_pixel((uint16_t)(cx - 26 - i * 3), (uint16_t)(cy + 12 - i * 2), 0xCCCCCC);
    }
}

static void draw_dealer_snatch(const Game *g, uint16_t cx) {
    int sa = g->snatch_anim;
    if (sa <= 0) return;
    const int half = SNATCH_DURATION - 33;     /* boundary descend<->retract */

    if (sa > half) {                            /* DESCEND: open claw drops in */
        int p = SNATCH_DURATION - sa;           /* 0..33 */
        int w = g->dealer_claw ? (int) g->dealer_claw->width  : 120;
        int h = g->dealer_claw ? (int) g->dealer_claw->height : 180;
        int x0 = (int) cx + 70,        y0 = -h;
        int x1 = (int) cx - w / 2,     y1 = 300 - h + 18;     /* claws into chips */
        int x = x0 + (x1 - x0) * p / 33;
        int y = y0 + (y1 - y0) * p / 33;
        if (g->dealer_claw && x >= 0 && y > -h)
            sprite_draw(g->dealer_claw, (uint16_t)(x < 0 ? 0 : x), (uint16_t)(y < 0 ? 0 : y));
    } else {                                    /* RETRACT: closed fist yanks up-left */
        int p = half - sa;                      /* 0..half */
        int w = g->dealer_fist ? (int) g->dealer_fist->width  : 100;
        int h = g->dealer_fist ? (int) g->dealer_fist->height : 100;
        int x0 = (int) cx - w / 2,  y0 = 300 - h + 30;
        int x1 = (int) cx - 160,    y1 = -h;
        int x = x0 + (x1 - x0) * p / half;
        int y = y0 + (y1 - y0) * p / half;
        if (g->dealer_fist && x >= 0 && y > -h)
            sprite_draw(g->dealer_fist, (uint16_t)(x < 0 ? 0 : x), (uint16_t)(y < 0 ? 0 : y));
    }

    if (sa <= SNATCH_DURATION - 27 && sa >= SNATCH_DURATION - 42)  /* the grab flash */
        draw_woosh((int) cx, 300);
}

/* ── Shadow-dealer eyes + typewriter speech (sudden, trembling) ──────────── */
static void draw_dealer_dialogue(const Game *g, uint16_t h_res) {
    if (g->dealer_line == NULL) return;
    int len   = (int) strlen(g->dealer_line);
    int shown = g->dealer_chars / DEALER_TYPE_DIV;
    if (shown > len) shown = len;
    bool typing = (shown < len);

    int tdx = 0, tdy = 0;                        /* tremble while speaking */
    if (typing) {
        tdx = (int)((unsigned)(g->dealer_chars * 7)  % 5u) - 2;
        tdy = (int)((unsigned)(g->dealer_chars * 13) % 5u) - 2;
    }

    int ew = g->dealer_eyes ? (int) g->dealer_eyes->width  : 280;
    int eh = g->dealer_eyes ? (int) g->dealer_eyes->height : 64;
    int fw = ew + 60, fh = eh + 34;
    int fx = (int) h_res / 2 - fw / 2, fy = 10;

    /* Dark face slab with a dim blood-red rim. */
    video_rounded_rect((uint16_t) fx, (uint16_t) fy, (uint16_t) fw, (uint16_t) fh,
                       10, 0x050507, 0x661111);
    if (g->dealer_eyes)
        sprite_draw(g->dealer_eyes, (uint16_t)(fx + 30 + tdx), (uint16_t)(fy + 16 + tdy));

    /* Speech box sized to the full line (so it doesn't grow as it types). */
    unsigned tw = text_width(g->dealer_line, 2);
    int bw = (int) tw + 24;
    int bx = (int) h_res / 2 - bw / 2;
    int by = fy + fh + 8;
    video_rounded_rect((uint16_t) bx, (uint16_t) by, (uint16_t) bw, 28, 6, 0x0A0A12, 0x884444);

    char buf[64];
    int k = shown < 63 ? shown : 63;
    for (int i = 0; i < k; i++) buf[i] = g->dealer_line[i];
    buf[k] = '\0';
    text_draw(buf, (uint16_t)(bx + 12), (uint16_t)(by + 7), 0xFF5555);
}

/* ── PURGATORY: the cursed menu glitch (a rare, quick flash) ─────────────── */
static void draw_cursed_menu(const Game *g, uint16_t h_res, uint16_t v_res) {
    unsigned seed = (unsigned) g->cursed_flash * 2654435761u + 7u;

    video_rect(0, 0, h_res, v_res, 0x050002);              /* near-black wash */

    for (int i = 0; i < 16; i++) {                          /* dark glitch bands */
        seed = seed * 1103515245u + 12345u;
        video_rect(0, (uint16_t)((seed >> 16) % v_res), h_res,
                   (uint16_t)(1 + ((seed >> 9) % 3)), 0x1A0000);
    }
    for (int i = 0; i < 3; i++) {                           /* bright tear lines */
        seed = seed * 1103515245u + 12345u;
        video_rect(0, (uint16_t)((seed >> 16) % v_res), h_res, 1, 0x550000);
    }

    int cx = (int) h_res / 2;
    int jx = (int)(seed % 3) - 1;                           /* tiny jitter (stays readable) */

    /* PURGATORY -- clean bright red with a dark-red drop shadow. */
    const char *t = "PURGATORY";
    int tw = (int) text_width(t, 4);
    int tx = cx - tw / 2, ty = 150;
    text_draw_scaled(t, (uint16_t)(tx + 3),  (uint16_t)(ty + 3), 0x550000, 4);  /* shadow */
    text_draw_scaled(t, (uint16_t)(tx + jx), (uint16_t) ty,      0xFF2222, 4);  /* core */

    /* SUFFER -- broken button box, bright red. */
    const char *s = "SUFFER";
    int sw = (int) text_width(s, 3);
    int sx = cx - sw / 2, sy = 398;
    video_rect((uint16_t)(sx - 22), (uint16_t)(sy - 12), (uint16_t)(sw + 44), 60, 0x180000);
    video_rect((uint16_t)(sx - 22), (uint16_t)(sy - 12), (uint16_t)(sw + 44), 1,  0x880000);
    video_rect((uint16_t)(sx - 22), (uint16_t)(sy + 14), (uint16_t)(sw + 44), 2,  0x000000);  /* crack */
    text_draw_scaled(s, (uint16_t)(sx + jx), (uint16_t) sy, 0xFF0000, 3);

    text_draw("STAY",    (uint16_t)(cx - (int) text_width("STAY", 2) / 2),    468, 0xCC2222);
    text_draw("NO EXIT", (uint16_t)(cx - (int) text_width("NO EXIT", 2) / 2), 508, 0xCC2222);
}

/* ── Context-sensitive keybind legend (bottom-right) ────────────────────── */
static void draw_keys(const Game *g, uint16_t h_res, uint16_t v_res) {
    const char *s;
    switch (g->state) {
        case STATE_MENU:     s = "SPACE PLAY   T SKIN   ESC QUIT";           break;
        case STATE_RULES:    s = "SPACE NEXT PAGE";                        break;
        case STATE_BET:      s = "UP/DOWN OR +/- BET   SPACE DEAL   ESC QUIT"; break;
        case STATE_PLAYER:   s = "SPACE HIT   BACKSPACE STAND   ESC QUIT";  break;
        case STATE_DEALER:   s = "DEALER DRAWING";                         break;
        case STATE_GAMEOVER: s = "CLICK TO CONTINUE";                      break;
        default:             s = "";                                       break;
    }
    if (s[0] == '\0') return;

    unsigned w = text_width(s, 2);
    uint16_t x = (w + 24u < (unsigned) h_res) ? (uint16_t)(h_res - w - 16) : 8;
    uint16_t y = (uint16_t)(v_res - 56);
    video_rect((uint16_t)(x - 8), (uint16_t)(y - 4), (uint16_t)(w + 16), 24, 0x001A1A);
    text_draw(s, x, y, 0x77CCCC);
}

/* ── Map an outcome to the flashing banner word + colour ────────────────── */
static const char *result_word(Outcome o, uint32_t *color) {
    switch (o) {
        case RESULT_PLAYER_WIN:
        case RESULT_DEALER_BUST: *color = COL_WIN;  return "WIN";
        case RESULT_PLAYER_BUST:
        case RESULT_DEALER_WIN:  *color = COL_LOSE; return "LOSE";
        case RESULT_PUSH:        *color = COL_PUSH; return "PUSH";
    }
    *color = 0xFFFFFF;
    return "";
}

/* ── Escalating dread: red glitch scanlines that multiply as you lose ────── */
static void draw_dread_overlay(const Game *g, uint16_t h_res, uint16_t v_res) {
    int d = g->dread;
    if (d <= 0) return;
    unsigned seed = s_rframe * 2654435761u + 101u;
    for (int i = 0; i < d * 3; i++) {
        seed = seed * 1103515245u + 12345u;
        video_rect(0, (uint16_t)((seed >> 16) % v_res), h_res, 1, 0x2A0000);
    }
    for (int i = 0; i < d / 2; i++) {            /* brighter tears at high dread */
        seed = seed * 1103515245u + 12345u;
        video_rect(0, (uint16_t)((seed >> 16) % v_res), h_res, 1, 0x550000);
    }
}

/* ── Main render ───────────────────────────────────────────────────────── */
void render_scene(const Game *g) {
    s_rframe++;
    video_gradient_apply();          /* baked felt table (border, rail, arcs) */

    uint16_t h_res = (uint16_t) video_width();
    uint16_t v_res = (uint16_t) video_height();
    uint16_t deck_x = 35, deck_y = 35;
    uint16_t cx = h_res / 2;

    bool table_active = (g->state == STATE_BET   || g->state == STATE_DEAL ||
                         g->state == STATE_PLAYER || g->state == STATE_DEALER ||
                         g->state == STATE_RESULT);

    /* Card zone overlays + table markings only while a hand is in play. */
    if (g->state == STATE_DEAL   || g->state == STATE_PLAYER ||
        g->state == STATE_DEALER || g->state == STATE_RESULT) {
        video_rounded_rect(cx - 160, 85,  320, 110, 8, 0x003333, 0x336666);
        video_rounded_rect(cx - 160, 355, 320, 110, 8, 0x003333, 0x336666);

        /* Casino rule markings -- only in Normal; Purgatory keeps the felt bare. */
        if (!g->mode->horror) {
            const char *bj = "BLACKJACK PAYS 3 TO 2";
            text_draw(bj, (h_res - text_width(bj, 2)) / 2, 50, 0x88AAAA);
            const char *rule = "DEALER MUST DRAW TO 16 AND STAND ON ALL 17S";
            text_draw(rule, (h_res - text_width(rule, 2)) / 2, 70, 0x668888);
        }

        draw_deck(deck_x, deck_y, g->card_back);
    }

    if (table_active) {
        draw_dealer_label(g);
        draw_player_box(g);
    }

    float progress = (g->anim_counter > 0)
                        ? 1.0f - (float)g->anim_counter / ANIM_DURATION
                        : 1.0f;

    switch (g->state) {
        case STATE_MENU: {
            /* Rare cursed flash: flicker in/out at the edges, hold solid and
             * readable in the middle. */
            if (g->cursed_flash > 0) {
                int  cf = g->cursed_flash;
                bool cursed = (cf >= CURSED_FLASH_LEN - 10 || cf < 8)
                                ? (((cf >> 1) & 1) == 0)   /* flicker at the edges */
                                : true;                    /* solid hold (readable) */
                if (cursed) { draw_cursed_menu(g, h_res, v_res); break; }
            }
            draw_emblem(h_res);
            const char *l1 = "ENTROPY AGGREGATOR";
            const char *l2 = "BLACKJACK";
            unsigned w1 = text_width(l1, 2);
            unsigned w2 = text_width(l2, 4);
            unsigned tw = w2 > w1 ? w2 : w1;
            uint16_t tx = (h_res - tw) / 2;
            uint16_t ty = 115;
            video_rounded_rect(tx - 24, ty - 16, tw + 48, 88, 10, 0x001122, COL_DIAMOND);
            text_draw_scaled(l1, tx + (tw - w1) / 2, ty, 0x88AAAA, 2);
            text_draw_scaled(l2, tx + (tw - w2) / 2, (uint16_t)(ty + 24), COL_DIAMOND, 4);
            video_rect(tx - 20, ty - 8, tw + 40, 1, COL_DIAMOND);
            video_rect(tx - 20, (uint16_t)(ty + 64), tw + 40, 1, COL_DIAMOND);
            button_draw(g->play);
            button_draw(g->exit);
            draw_book_icon(g->rules);
            draw_skin_icon(g->skin_btn, g->skin_index);
            draw_menu_chips(h_res, v_res);
            break;
        }

        case STATE_RULES: {
            uint16_t rw = 600, rh = 350;   /* wider so the longest lines fit inside */
            uint16_t rx = cx - rw / 2;
            uint16_t ry = 100;
            uint16_t tx = rx + 30;

            if (g->rules_page == 0) {
                video_rounded_rect(rx, ry, rw, rh, 10, 0x001122, COL_GOLD);
                text_draw("RULES OF BLACKJACK",
                          cx - text_width("RULES OF BLACKJACK", 2) / 2, ry + 22, 0xFFFF00);
                video_rect(rx + 20, ry + 50, rw - 40, 1, COL_GOLD);
                const char *bullets[] = {
                    "BEAT THE DEALER WITHOUT GOING OVER 21",
                    "FACE CARDS (J/Q/K) ARE WORTH 10",
                    "ACE IS WORTH 11 OR 1",
                    "DRAW (HIT) TO GET CLOSER TO 21",
                    "STAND TO STOP DRAWING",
                    "DEALER MUST DRAW TO 16 AND STAND ON ALL 17S",
                    "NATURAL 21 ON FIRST 2 CARDS IS BLACKJACK",
                    "BLACKJACK PAYS 3 TO 2",
                    "GOING OVER 21 BUSTS AND YOU LOSE",
                    "TIES ARE A PUSH (BET RETURNED)"
                };
                for (int i = 0; i < 10; i++)
                    text_draw(bullets[i], tx, (uint16_t)(ry + 70 + i * 20), 0xCCCCCC);
                video_rect(rx + 20, ry + 285, rw - 40, 1, COL_GOLD);
                const char *nav = "PRESS SPACE OR CLICK FOR BETTING AND FAIRNESS >";
                text_draw(nav, cx - text_width(nav, 2) / 2, ry + 305, 0x888888);
                text_draw("PAGE 1/2", rx + 15, ry + rh - 20, 0x666666);
            } else {
                video_rounded_rect(rx, ry, rw, rh, 10, 0x001C22, 0x44AAAA);
                text_draw("BETTING AND FAIRNESS",
                          cx - text_width("BETTING AND FAIRNESS", 2) / 2, ry + 16, 0x44CCCC);
                video_rect(rx + 20, ry + 40, rw - 40, 1, 0x44AAAA);
                text_draw("HOW TO BET", tx, ry + 50, COL_DIAMOND);
                const char *sec1[] = {
                    "YOU AND THE DEALER BOTH START WITH A PURSE",
                    "PICK YOUR WAGER WITH + / - OR UP / DOWN",
                    "MIN BET IS 5  -  LOW FUNDS FORCES ALL IN",
                    "WIN TAKES FROM THE DEALER  -  LOSE PAYS IT"
                };
                for (int i = 0; i < 4; i++)
                    text_draw(sec1[i], tx, (uint16_t)(ry + 70 + i * 16), 0xCCCCCC);
                text_draw("PROVABLY FAIR", tx, (uint16_t)(ry + 148), COL_DIAMOND);
                const char *sec2[] = {
                    "EVERY SHUFFLE IS SEEDED FROM LIVE ENTROPY",
                    "TIMER  KEYBOARD  MOUSE AND RTC FEED THE POOL",
                    "NEED 64 EVENTS BEFORE THE FIRST DEAL",
                    "EACH DECK IS A SHA-256 SEED  -  NO RIGGING",
                    "EMPTY A PURSE TO END THE GAME"
                };
                for (int i = 0; i < 5; i++)
                    text_draw(sec2[i], tx, (uint16_t)(ry + 168 + i * 16), 0xCCCCCC);
                video_rect(rx + 20, ry + 295, rw - 40, 1, 0x44AAAA);
                const char *nav = "< PRESS SPACE OR CLICK TO RETURN";
                text_draw(nav, cx - text_width(nav, 2) / 2, ry + 315, 0x888888);
                text_draw("PAGE 2/2", rx + 15, ry + rh - 20, 0x666666);
            }
            break;
        }

        case STATE_BET: {
            uint16_t pw = 460, ph = 232;
            uint16_t px = cx - pw / 2, py = 180;
            video_rounded_rect(px, py, pw, ph, 12, 0x001122, COL_GOLD);

            const char *title = "PLACE YOUR BET";
            text_draw(title, cx - text_width(title, 2) / 2, py + 10, COL_DIAMOND);
            video_rect(px + 24, py + 30, pw - 48, 1, COL_GOLD);

            /* Big current-bet number, flanked by the -/+ buttons. */
            char buf[8];
            sprintf(buf, "%d", g->bet);
            text_draw_scaled(buf, cx - text_width(buf, 5) / 2, 224, 0xFFFFFF, 5);
            button_draw(g->bet_minus);
            button_draw(g->bet_plus);
            button_draw(g->allin);
            button_draw(g->deal);
            button_draw(g->menu_btn);

            const char *hint = (g->balance < MIN_BET)
                                 ? "LOW FUNDS  -  FORCED ALL IN"
                                 : "UP/DOWN OR +/- TO BET    SPACE TO DEAL";
            text_draw(hint, cx - text_width(hint, 2) / 2, py + ph - 22, 0x99AAAA);
            break;
        }

        case STATE_DEAL:
        case STATE_PLAYER:
        case STATE_DEALER:
        case STATE_RESULT: {
            /* Cards + scores drawn FIRST (bottom layer). */
            if (g->state != STATE_DEAL) {
                bool hide_dealer_hole = (g->state == STATE_PLAYER);
                draw_fanned_hand(&g->dealer, cx, 100, hide_dealer_hole,
                                 g->card_face, g->card_back, progress, deck_x, deck_y,
                                 g->anim_settled_dealer);
                draw_fanned_hand(&g->player, cx, 370, false,
                                 g->card_face, g->card_back, progress, deck_x, deck_y,
                                 g->anim_settled_player);
                draw_score_bubble(&g->dealer, cx, 185, 0xFFFFFF, hide_dealer_hole);
                draw_score_bubble(&g->player, cx, 455, 0xFFFFFF, false);
            }

            /* Hit/Stand buttons above cards. */
            if (g->state == STATE_PLAYER) {
                button_draw(g->hit);
                button_draw(g->stand);
            }

            /* Flashing WIN / LOSE / PUSH banner above cards & buttons. */
            if (g->state == STATE_RESULT && ((g->result_flash / 8) % 2) == 0) {
                uint32_t col;
                const char *w = result_word(g->outcome, &col);
                text_draw_scaled(w, cx - text_width(w, 6) / 2, 250, col, 6);
            }

            /* Static chips drawn FIRST so the arm/hand covers them at its
             * position. Each stack disappears individually when the hand
             * grabs it (70 = first grab, 30 = second grab). */
            if (!g->chips_taken) {
                bool left_taken = false, right_taken = false;
                if (g->state == STATE_RESULT) {
                    if (g->chip_anim > 0) {
                        /* Player win: grabs left first, then right */
                        left_taken  = (g->chip_anim <= 70);
                        right_taken = (g->chip_anim <= 30);
                    } else if (g->dealer_win_snatch > 0) {
                        /* Dealer win: grabs right first, then left */
                        left_taken  = (g->dealer_win_snatch <= 30);
                        right_taken = (g->dealer_win_snatch <= 70);
                    }
                }
                bool placed = (g->state != STATE_DEAL || g->chip_anim <= 60);
                if (placed) {
                    if (!left_taken)
                        draw_wager_chips((uint16_t)((int)cx - 55), 300, g->bet);
                    if (!right_taken)
                        draw_wager_chips((uint16_t)((int)cx + 55), 300, g->bet);
                }
            }

            /* Chip/hand animations drawn ON TOP of static chips. */
            if (g->chip_anim > 0) {
                draw_player_delivery(g, cx, v_res);
                if (g->state == STATE_DEAL) {
                    if (g->mode->horror)
                        draw_dealer_drop(g, cx, v_res);
                    else
                        draw_dealer_delivery(g, cx, v_res);
                }
            }
            if (g->dealer_win_snatch > 0 && !g->mode->horror)
                draw_dealer_win_collect(g, cx, v_res);
            if (g->snatch_anim > 0)
                draw_dealer_snatch(g, cx);
            break;
        }

        case STATE_GAMEOVER: {
            if (!g->game_won && g->mode->horror) {
                /* PURGATORY LOSS: aggressive black strobe with the dealer's eyes
                 * looming ever larger, then back to the menu. (Normal mode falls
                 * through to the plain GAME OVER panel below.) */
                int ot = g->over_tick;
                bool black = ((ot >> 1) & 1) == 0;
                video_rect(0, 0, h_res, v_res, black ? 0x000000 : 0x0A0000);
                if (!black) {
                    int grow = ot < 150 ? ot : 150;        /* 1x .. ~2.5x */
                    draw_sprite_scaled(g->dealer_eyes, cx, v_res / 2 - 30, 100 + grow, 100);
                    if (g->dealer_line[0] != '\0') {
                        const char *l = g->dealer_line;
                        text_draw(l, (uint16_t)(cx - (int) text_width(l, 2) / 2),
                                  (uint16_t)(v_res - 90), 0xCC0000);
                    }
                }
                break;
            }
            draw_emblem(h_res);
            uint16_t pw = 460, ph = 220;
            uint16_t px = cx - pw / 2, py = 190;
            uint32_t border = g->game_won ? COL_GOLD : COL_LOSE;
            uint32_t fill   = g->game_won ? 0x001A00 : 0x1A0000;
            video_rounded_rect(px, py, pw, ph, 12, fill, border);

            const char *t = g->game_won ? "GAME WON" : "GAME LOST";
            uint32_t tcol = g->game_won ? COL_WIN : COL_LOSE;
            text_draw_scaled(t, cx - text_width(t, 4) / 2, py + 30, tcol, 4);

            char buf[28];
            sprintf(buf, "ROUNDS PLAYED %d", g->round);
            text_draw(buf, cx - text_width(buf, 2) / 2, py + 95, 0xCCCCCC);
            sprintf(buf, "FINAL WALLET$ %d", g->balance);
            text_draw(buf, cx - text_width(buf, 2) / 2, py + 120, 0x33DD33);

            const char *c = "CLICK TO RETURN TO MENU";
            text_draw(c, cx - text_width(c, 2) / 2, py + 170, 0x99AAAA);
            break;
        }
    }

    /* Escalating dread glitch -- horror only, while a hand is going. */
    if (g->mode->horror &&
        (g->state == STATE_BET    || g->state == STATE_DEAL   ||
         g->state == STATE_PLAYER || g->state == STATE_DEALER ||
         g->state == STATE_RESULT))
        draw_dread_overlay(g, h_res, v_res);

    /* The shadow dealer looms over everything when he speaks. */
    if (g->dealer_show)
        draw_dealer_dialogue(g, h_res);

    draw_keys(g, h_res, v_res);
    sprite_draw(g->cursor, g->cursor_x, g->cursor_y);
}
