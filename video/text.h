#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

/* Tiny 5x7 bitmap font. Glyphs exist for 0-9, A-Z, space, ':' and '!';
 * everything else renders blank. */

void     text_draw(const char *s, uint16_t x, uint16_t y, uint32_t color);
void     text_draw_scaled(const char *s, uint16_t x, uint16_t y,
                          uint32_t color, uint8_t scale);
unsigned text_width(const char *s, uint8_t scale);

/* Retro-arcade wobble: set the current animation phase (advance it once per
 * rendered frame). The ripple/jitter only applies to text drawn between
 * text_anim_begin() and text_anim_end() -- used for clickable button labels. */
void     text_set_anim(int phase);
void     text_anim_begin(void);
void     text_anim_end(void);

/* Draws a card's rank+suit label at (x,y): red for hearts/diamonds, black for
 * spades/clubs. `card` is 0..51 (rank = card%13, suit = card/13, order SHDC). */
void     text_card(uint8_t card, uint16_t x, uint16_t y);

#endif
