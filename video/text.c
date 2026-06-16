#include <stdint.h>
#include <stdbool.h>

#include "text.h"
#include "video.h"

#define GLYPH_W 5
#define GLYPH_H 7
#define ADVANCE 6          /* glyph width + 1 px gap, in unscaled pixels */

/* ── Retro-arcade text wobble ───────────────────────────────────────────────
 * A small travelling sine-ish wave indexed by the glyph's x position plus a
 * global phase, so text ripples vertically; plus an occasional 1px horizontal
 * jolt for that unstable old-CRT feel. Only applied between text_anim_begin()
 * and text_anim_end() -- i.e. only on clickable (button) text. Integer-only. */
static const int8_t WOBBLE[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
static int  s_anim_phase = 0;
static bool s_anim_on     = false;

void text_set_anim(int phase) { s_anim_phase = phase; }
void text_anim_begin(void)    { s_anim_on = true;  }
void text_anim_end(void)      { s_anim_on = false; }

/* Each glyph is 7 rows; the low 5 bits of each byte are the pixels, MSB-left.
 * Indexed by (ASCII - 32), covering ' '(32) .. 'Z'(90). Unlisted entries are
 * all-zero (blank). */
static const uint8_t font[59][GLYPH_H] = {
    ['!' - 32] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
    ['/' - 32] = {0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x00},
    [':' - 32] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    ['$' - 32] = {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},
    ['+' - 32] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    ['-' - 32] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},

    ['0' - 32] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    ['1' - 32] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['2' - 32] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    ['3' - 32] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    ['4' - 32] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    ['5' - 32] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    ['6' - 32] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    ['7' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    ['8' - 32] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    ['9' - 32] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},

    ['A' - 32] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['B' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    ['C' - 32] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    ['D' - 32] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    ['E' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    ['F' - 32] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    ['G' - 32] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    ['H' - 32] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    ['I' - 32] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    ['J' - 32] = {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    ['K' - 32] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    ['L' - 32] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    ['M' - 32] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    ['N' - 32] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    ['O' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['P' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    ['Q' - 32] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    ['R' - 32] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    ['S' - 32] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    ['T' - 32] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    ['U' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    ['V' - 32] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    ['W' - 32] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
    ['X' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    ['Y' - 32] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    ['Z' - 32] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
};

static void draw_glyph(const uint8_t g[GLYPH_H], uint16_t x, uint16_t y,
                       uint32_t color, uint8_t scale) {
    /* Per-glyph vertical ripple (phase scrolls with x) + rare horizontal jolt,
     * but only while animation is enabled (clickable text). */
    int oy = 0, ox = 0;
    if (s_anim_on) {
        oy = WOBBLE[((x >> 3) + s_anim_phase) & 7];
        ox = ((s_anim_phase % 11) == 0) ? 1 : 0;
    }
    int bx = (int) x + ox;
    int by = (int) y + oy;
    for (int row = 0; row < GLYPH_H; row++)
        for (int col = 0; col < GLYPH_W; col++)
            if (g[row] & (1 << (GLYPH_W - 1 - col)))
                video_rect((uint16_t)(bx + col * scale), (uint16_t)(by + row * scale),
                           scale, scale, color);
}

void text_draw_scaled(const char *s, uint16_t x, uint16_t y,
                      uint32_t color, uint8_t scale) {
    uint16_t cx = x;
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if (c >= 32 && c <= 90)
            draw_glyph(font[c - 32], cx, y, color, scale);
        cx += ADVANCE * scale;
    }
}

void text_draw(const char *s, uint16_t x, uint16_t y, uint32_t color) {
    text_draw_scaled(s, x, y, color, 2);
}

unsigned text_width(const char *s, uint8_t scale) {
    unsigned n = 0;
    while (s[n] != '\0')
        n++;
    return n * ADVANCE * scale;
}

/* Suit pixel patterns — each uint8_t bitmask is 7 rows × 7 columns,
 * bit 6 = leftmost column, bit 0 = rightmost column, 1 = fill. */
static void draw_suit_symbol(uint16_t x, uint16_t y, uint8_t suit, uint32_t color, uint8_t scale) {
    static const uint8_t patterns[4][7] = {
        {0x14, 0x1C, 0x3E, 0x7F, 0x7F, 0x3E, 0x14}, /* spade */
        {0x36, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C, 0x08}, /* heart */
        {0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08}, /* diamond */
        {0x14, 0x1C, 0x3E, 0x7F, 0x7F, 0x3E, 0x22}, /* club */
    };
    if (suit > 3) return;
    for (int row = 0; row < 7; row++) {
        uint8_t bits = patterns[suit][row];
        for (int col = 0; col < 7; col++)
            if (bits & (1 << (6 - col)))
                video_rect(x + col * scale, y + row * scale, scale, scale, color);
    }
}

/* Predefined pip position tables for numbered cards 2-10 (index = count-2).
 * Each is a (col, row) pair in a 3-col/4-row grid (col 0-2, row 0-3).
 * Converted to pixel: px = x + 12 + col*14,  py = y + 21 + row*14. */
static const int8_t pip_layouts[9][10][2] = {
    {{1,0},{1,2},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},             /* 2 */
    {{1,0},{1,1},{1,2},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},             /* 3 */
    {{0,0},{2,0},{0,2},{2,2},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},             /* 4 */
    {{0,0},{2,0},{1,1},{0,2},{2,2},{0,0},{0,0},{0,0},{0,0},{0,0}},             /* 5 */
    {{0,0},{2,0},{0,1},{2,1},{0,2},{2,2},{0,0},{0,0},{0,0},{0,0}},             /* 6 */
    {{0,0},{2,0},{0,1},{1,1},{2,1},{0,2},{2,2},{0,0},{0,0},{0,0}},             /* 7 */
    {{0,0},{1,0},{2,0},{0,1},{2,1},{0,2},{1,2},{2,2},{0,0},{0,0}},             /* 8 */
    {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2},{2,2},{0,0}},             /* 9 */
    {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2},{2,2},{1,3}},             /* 10 */
};

void text_card(uint8_t card, uint16_t x, uint16_t y) {
    static const char ranks[] = "A23456789TJQK";
    uint8_t r = card % 13;
    uint8_t s = card / 13;
    uint32_t color = (s == 1 || s == 2) ? 0xCC0000 : 0x000000;
    char rank_str[2] = { ranks[r], '\0' };
    unsigned rw = text_width(rank_str, 2);

    /* Corner indicators — top-left: rank then suit, bottom-aligned */
    text_draw_scaled(rank_str, x + 3, y + 3, color, 2);
    draw_suit_symbol(x + 3 + rw + 2, y + 10, s, color, 1);

    /* Bottom-right: suit then rank, bottom-aligned */
    draw_suit_symbol(x + 60 - 3 - rw - 2 - 7, y + 70, s, color, 1);
    text_draw_scaled(rank_str, x + 60 - 3 - rw, y + 63, color, 2);

    /* Center decoration: Ace gets a large suit pip; 2-10 get pips in the
     * traditional layout; face cards (J/Q/K) get a large rank letter. */
    if (r == 0) {                                 /* Ace */
        draw_suit_symbol(x + (60 - 14) / 2, y + (80 - 14) / 2, s, color, 2);
    } else if (r >= 10) {                          /* Jack, Queen, King */
        unsigned rw4 = text_width(rank_str, 4);
        text_draw_scaled(rank_str, x + (60 - rw4) / 2, y + 26, color, 4);
        draw_suit_symbol(x + (60 - 7) / 2, y + 56, s, color, 1);
    } else {                                        /* 2 – 10 */
        int count = r + 1;                         /* rank 1→2 pips … rank 9→10 */
        for (int i = 0; i < count; i++) {
            uint16_t px = (uint16_t)(x + 12 + pip_layouts[count - 2][i][0] * 14);
            uint16_t py = (uint16_t)(y + 21 + pip_layouts[count - 2][i][1] * 14);
            draw_suit_symbol(px, py, s, color, 1);
        }
    }
}
