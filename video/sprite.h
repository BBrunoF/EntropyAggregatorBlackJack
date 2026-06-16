#ifndef SPRITE_H
#define SPRITE_H

#include <lcom/lcf.h>
#include <stdint.h>

/* A decoded XPM image ready to blit. */
typedef struct {
    uint16_t  width, height;
    uint32_t *pixmap;       /* 0xRRGGBB per pixel, row-major */
    uint32_t  transparent;  /* pixels equal to this are not drawn */
} Sprite;

Sprite *sprite_load(xpm_map_t xpm);
void    sprite_draw(const Sprite *sp, uint16_t x, uint16_t y);
void    sprite_draw_ex(const Sprite *sp, uint16_t x, int y,
                       bool flip_h, bool flip_v,
                       uint32_t tint_dark, uint32_t tint_light,
                       uint8_t threshold);
void    sprite_free(Sprite *sp);

#endif
