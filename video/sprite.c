#include <lcom/lcf.h>
#include <stdint.h>
#include <stdlib.h>

#include "sprite.h"
#include "video.h"

Sprite *sprite_load(xpm_map_t xpm) {
    xpm_image_t img;
    uint8_t *raw = xpm_load(xpm, XPM_8_8_8, &img);
    if (raw == NULL)
        return NULL;

    Sprite *sp = malloc(sizeof *sp);
    if (sp == NULL)
        return NULL;

    sp->width       = img.width;
    sp->height      = img.height;
    sp->transparent = xpm_transparency_color(XPM_8_8_8);
    sp->pixmap      = malloc((size_t) img.width * img.height * sizeof(uint32_t));
    if (sp->pixmap == NULL) {
        free(sp);
        return NULL;
    }

    /* xpm_load gives B,G,R byte triplets; pack them into 0xRRGGBB. */
    int count = img.width * img.height;
    for (int i = 0; i < count; i++) {
        uint8_t *p = &raw[i * 3];
        sp->pixmap[i] = ((uint32_t) p[2] << 16) | ((uint32_t) p[1] << 8) | p[0];
    }
    return sp;
}

void sprite_draw(const Sprite *sp, uint16_t x, uint16_t y) {
    if (sp == NULL)
        return;
    video_blit(sp->pixmap, sp->width, sp->height, x, y, sp->transparent);
}

static int pixel_brightness(uint32_t c) {
    return (int)(((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF)) / 3;
}

void sprite_draw_ex(const Sprite *sp, uint16_t x, int y,
                    bool flip_h, bool flip_v,
                    uint32_t tint_dark, uint32_t tint_light,
                    uint8_t threshold) {
    if (sp == NULL) return;
    int w = (int) sp->width, h = (int) sp->height;
    uint32_t trans = sp->transparent;
    int rd = (int)((tint_dark >> 16) & 0xFF);
    int gd = (int)((tint_dark >>  8) & 0xFF);
    int bd = (int)( tint_dark        & 0xFF);
    int rl = (int)((tint_light >> 16) & 0xFF);
    int gl = (int)((tint_light >>  8) & 0xFF);
    int bl = (int)( tint_light        & 0xFF);
    bool do_dual = (threshold > 0);
    int sw = (int) video_width(), sh = (int) video_height();
    for (int j = 0; j < h; j++) {
        int sy = flip_v ? (h - 1 - j) : j;
        int yy = (int) y + j;
        if (yy < 0 || yy >= sh) continue;
        for (int i = 0; i < w; i++) {
            int sx = flip_h ? (w - 1 - i) : i;
            uint32_t c = sp->pixmap[(uint32_t)sy * w + sx];
            if (c == trans) continue;
            if (do_dual) {
                int bright = pixel_brightness(c);
                int r, g, b;
                if (bright < threshold) {
                    r = (int)((c >> 16) & 0xFF) * rd / 255;
                    g = (int)((c >>  8) & 0xFF) * gd / 255;
                    b = (int)( c        & 0xFF) * bd / 255;
                } else {
                    r = (int)((c >> 16) & 0xFF) * rl / 255;
                    g = (int)((c >>  8) & 0xFF) * gl / 255;
                    b = (int)( c        & 0xFF) * bl / 255;
                }
                if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
                c = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
            int xx = (int) x + i;
            if (xx < 0 || xx >= sw) continue;
            video_pixel((uint16_t) xx, (uint16_t) yy, c);
        }
    }
}

void sprite_free(Sprite *sp) {
    if (sp == NULL)
        return;
    free(sp->pixmap);
    free(sp);
}
