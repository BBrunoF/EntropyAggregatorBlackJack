#include <stdlib.h>

#include "button.h"
#include "video.h"
#include "text.h"

#define BORDER 0xFFFFFF
#define LABEL  0xFFFFFF
#define LABEL_SCALE 2

Button *button_create(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const char *label, uint32_t base, uint32_t hover) {
    Button *b = malloc(sizeof *b);
    if (b == NULL)
        return NULL;
    b->x = x; b->y = y; b->w = w; b->h = h;
    b->label = label;
    b->base = base; b->hover = hover;
    b->hovered = false;
    b->anim_on_hover = false;
    return b;
}

bool button_contains(const Button *b, uint16_t mx, uint16_t my) {
    return b != NULL &&
           mx >= b->x && mx < b->x + b->w &&
           my >= b->y && my < b->y + b->h;
}

bool button_update(Button *b, uint16_t mx, uint16_t my) {
    if (b == NULL)
        return false;
    bool was = b->hovered;
    b->hovered = button_contains(b, mx, my);
    return b->hovered != was;
}

void button_draw(const Button *b) {
    if (b == NULL)
        return;

    video_rect(b->x, b->y, b->w, b->h, b->hovered ? b->hover : b->base);

    /* 1px border */
    video_rect(b->x, b->y, b->w, 1, BORDER);
    video_rect(b->x, b->y + b->h - 1, b->w, 1, BORDER);
    video_rect(b->x, b->y, 1, b->h, BORDER);
    video_rect(b->x + b->w - 1, b->y, 1, b->h, BORDER);

    /* centered label (wobbles on hover if anim_on_hover is set) */
    unsigned tw = text_width(b->label, LABEL_SCALE);
    unsigned th = 7 * LABEL_SCALE;
    uint16_t tx = b->x + (b->w > tw ? (uint16_t) ((b->w - tw) / 2) : 0);
    uint16_t ty = b->y + (b->h > th ? (uint16_t) ((b->h - th) / 2) : 0);
    if (b->anim_on_hover && b->hovered)
        text_anim_begin();
    text_draw_scaled(b->label, tx, ty, LABEL, LABEL_SCALE);
    if (b->anim_on_hover && b->hovered)
        text_anim_end();
}

void button_free(Button *b) {
    free(b);
}
