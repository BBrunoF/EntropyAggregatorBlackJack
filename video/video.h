#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

/* Off-screen-buffered VBE graphics.
 *
 * video_init() sets the mode, maps VRAM, and allocates a back buffer. Every
 * draw call writes to the back buffer only; video_flush_diff() copies the
 * changed scanlines to VRAM (double buffering -> no tearing). */

int  video_init(uint16_t mode);
void video_exit(void);

unsigned video_width(void);
unsigned video_height(void);

/* Drawing (all bounds-clipped, all to the back buffer). Colors are 0xRRGGBB. */
void video_clear(uint32_t color);
void video_pixel(uint16_t x, uint16_t y, uint32_t color);
void video_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);

/* Blit a w*h pixmap (one 0xRRGGBB per entry, row-major) at (x,y), skipping any
 * pixel equal to `transparent`. */
void video_blit(const uint32_t *pixmap, uint16_t w, uint16_t h,
                uint16_t x, uint16_t y, uint32_t transparent);

/* Present the back buffer by copying ONLY the scanlines that actually changed
 * since the previous present. A shadow copy of what is currently on screen is
 * compared (in cheap system RAM) row by row against the back buffer; only the
 * differing rows are written to VRAM. Writing to the (emulated) framebuffer is
 * the single host-dependent cost in the render loop, so minimising it keeps the
 * loop fast enough to service the mouse without backlog. The first call paints
 * the whole screen. */
void video_flush_diff(void);

/* Precomputed radial gradient felt. Call once after video_init(). */
void video_gradient_precompute(uint32_t center_color, uint32_t edge_color);
void video_gradient_apply(void);
void video_gradient_free(void);

/* Snapshot the current back buffer into the gradient buffer, so future
 * video_gradient_apply() calls restore it. Lets a static backdrop (e.g. the
 * felt table) be drawn once and then repainted for free every frame. */
void video_gradient_capture(void);

/* Rounded rectangle: filled fill_color with optional border_color border. */
int  video_rounded_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        uint16_t radius, uint32_t fill, uint32_t border);

#endif
