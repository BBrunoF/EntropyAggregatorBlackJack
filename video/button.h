#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

/* A clickable rectangle with a centered text label and a hover color. Drawn
 * with video/text primitives -- no sprite needed. */
typedef struct {
    uint16_t    x, y, w, h;
    const char *label;
    uint32_t    base, hover;   /* fill colors: idle / hovered */
    bool        hovered;
    bool        anim_on_hover; /* wobble animation while hovered */
} Button;

Button *button_create(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const char *label, uint32_t base, uint32_t hover);
bool    button_contains(const Button *b, uint16_t mx, uint16_t my);
/* Updates `hovered`; returns true if it changed (i.e. the button needs redraw). */
bool    button_update(Button *b, uint16_t mx, uint16_t my);
void    button_draw(const Button *b);
void    button_free(Button *b);

#endif
