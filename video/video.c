#include <lcom/lcf.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "video.h"

static unsigned  scr_w, scr_h, bpp;
static uint8_t  *vram;
static uint8_t  *back;
static uint8_t  *shadow;          /* mirror of what is currently on screen */
static bool      shadow_valid;    /* false until the first full present */
static uint8_t  *gradient_buf;
static size_t    gradient_buf_size;

int video_init(uint16_t mode) {
    vbe_mode_info_t info;
    memset(&info, 0, sizeof info);
    if (vbe_get_mode_info(mode, &info) != 0)
        return 1;

    scr_w = info.XResolution;
    scr_h = info.YResolution;
    bpp   = (info.BitsPerPixel + 7) / 8;

    unsigned size = scr_w * scr_h * bpp;

    struct minix_mem_range mr;
    mr.mr_base  = (phys_bytes) info.PhysBasePtr;
    mr.mr_limit = mr.mr_base + size;
    if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK)
        return 1;

    vram = vm_map_phys(SELF, (void *) (uintptr_t) info.PhysBasePtr, size);
    if (vram == MAP_FAILED)
        return 1;

    back = malloc(size);
    if (back == NULL)
        return 1;
    memset(back, 0, size);

    /* Shadow of on-screen contents for diff-based presenting. Optional: if the
     * allocation fails we simply fall back to full-screen flushes. */
    shadow = malloc(size);
    shadow_valid = false;

    reg86_t r;
    memset(&r, 0, sizeof r);
    r.intno = 0x10;
    r.ah = 0x4F;
    r.al = 0x02;
    r.bx = mode | BIT(14);          /* set mode, linear framebuffer */
    if (sys_int86(&r) != OK)
        return 1;

    return 0;
}

void video_exit(void) {
    video_gradient_free();
    if (back != NULL) {
        free(back);
        back = NULL;
    }
    if (shadow != NULL) {
        free(shadow);
        shadow = NULL;
    }
    shadow_valid = false;
    vg_exit();
}

unsigned video_width(void)  { return scr_w; }
unsigned video_height(void) { return scr_h; }

/* Convert a 0xRRGGBB color to the active mode's native pixel value: identity for
 * 24bpp (low 3 bytes), packed 5:6:5 for 16bpp. */
static uint32_t pack_color(uint32_t rgb) {
    if (bpp == 2) {
        uint32_t r = (rgb >> 16) & 0xFF;
        uint32_t g = (rgb >>  8) & 0xFF;
        uint32_t b =  rgb        & 0xFF;
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
    return rgb;
}

/* Write `n` consecutive pixels of the already-packed value `native` starting at
 * `p`, with no per-pixel function call, bounds check or memcpy. The byte-wise
 * 24bpp path matches the B,G,R order the rest of the code writes. */
static inline void fill_pixels(uint8_t *p, unsigned n, uint32_t native) {
    switch (bpp) {
        case 4: {
            uint32_t *q = (uint32_t *) p;
            while (n--) *q++ = native;
            break;
        }
        case 2: {
            uint16_t v = (uint16_t) native, *q = (uint16_t *) p;
            while (n--) *q++ = v;
            break;
        }
        case 3: {
            uint8_t b0 = (uint8_t) native, b1 = (uint8_t)(native >> 8), b2 = (uint8_t)(native >> 16);
            while (n--) { p[0] = b0; p[1] = b1; p[2] = b2; p += 3; }
            break;
        }
        default: {
            uint8_t v = (uint8_t) native;
            while (n--) *p++ = v;
            break;
        }
    }
}

void video_pixel(uint16_t x, uint16_t y, uint32_t color) {
    if (x >= scr_w || y >= scr_h)
        return;
    fill_pixels(&back[(y * scr_w + x) * bpp], 1, pack_color(color));
}

void video_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (w == 0 || h == 0 || x >= scr_w || y >= scr_h)
        return;
    /* Clip once, up front, instead of bounds-checking every pixel. */
    unsigned x1 = (unsigned) x + w; if (x1 > scr_w) x1 = scr_w;
    unsigned y1 = (unsigned) y + h; if (y1 > scr_h) y1 = scr_h;
    unsigned cw = x1 - x;
    if (cw == 0)
        return;

    uint32_t native = pack_color(color);

    /* Fill the first scanline directly, then memcpy it down to the rest -- one
     * bulk copy per row instead of w*h pixel writes. */
    uint8_t *row0 = &back[((size_t)(unsigned) y * scr_w + x) * bpp];
    fill_pixels(row0, cw, native);
    size_t row_bytes = (size_t) cw * bpp;
    for (unsigned j = (unsigned) y + 1; j < y1; j++)
        memcpy(&back[((size_t) j * scr_w + x) * bpp], row0, row_bytes);
}

void video_clear(uint32_t color) {
    unsigned total = scr_w * scr_h * bpp;
    if (total == 0)
        return;
    /* Write the first pixel, then keep doubling it across the buffer. */
    uint32_t native = pack_color(color);
    memcpy(back, &native, bpp);
    unsigned done = bpp;
    while (done < total) {
        unsigned chunk = (done < total - done) ? done : total - done;
        memcpy(back + done, back, chunk);
        done += chunk;
    }
}

void video_blit(const uint32_t *pixmap, uint16_t w, uint16_t h,
                uint16_t x, uint16_t y, uint32_t transparent) {
    /* Per-row: clip the visible span once, then write opaque pixels straight to
     * the back buffer (no per-pixel function call or memcpy). Transparency is
     * checked per pixel, so rows can't be bulk-copied like a solid rect. */
    unsigned xmax = (x + w <= scr_w) ? w : (scr_w > x ? scr_w - x : 0);
    for (uint16_t j = 0; j < h; j++) {
        unsigned py = (unsigned) y + j;
        if (py >= scr_h)
            break;
        const uint32_t *src = &pixmap[(size_t) j * w];
        uint8_t *dst = &back[((size_t) py * scr_w + x) * bpp];
        for (unsigned i = 0; i < xmax; i++) {
            uint32_t c = src[i];
            if (c == transparent)
                continue;
            fill_pixels(dst + (size_t) i * bpp, 1, pack_color(c));
        }
    }
}

void video_gradient_precompute(uint32_t center_color, uint32_t edge_color) {
    gradient_buf_size = (size_t)scr_w * scr_h * bpp;
    gradient_buf = (uint8_t *) malloc(gradient_buf_size);
    if (gradient_buf == NULL) return;

    unsigned cx = scr_w / 2;
    unsigned cy = scr_h / 2;
    unsigned max_r_sq = cx * cx + cy * cy;

    uint8_t c_r = (center_color >> 16) & 0xFF;
    uint8_t c_g = (center_color >> 8)  & 0xFF;
    uint8_t c_b =  center_color        & 0xFF;
    uint8_t e_r = (edge_color >> 16) & 0xFF;
    uint8_t e_g = (edge_color >> 8)  & 0xFF;
    uint8_t e_b =  edge_color        & 0xFF;

    for (unsigned y = 0; y < scr_h; y++) {
        for (unsigned x = 0; x < scr_w; x++) {
            int dx = (int)x - (int)cx;
            int dy = (int)y - (int)cy;
            unsigned dist_sq = (unsigned)(dx*dx + dy*dy);
            uint32_t t = (dist_sq * 255) / max_r_sq;
            if (t > 255) t = 255;

            uint8_t r = (uint8_t)(c_r + (((int)e_r - (int)c_r) * (int)t / 255));
            uint8_t g = (uint8_t)(c_g + (((int)e_g - (int)c_g) * (int)t / 255));
            uint8_t b = (uint8_t)(c_b + (((int)e_b - (int)c_b) * (int)t / 255));

            unsigned off = (y * scr_w + x) * bpp;
            if (bpp == 2) {
                uint32_t packed = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                memcpy(&gradient_buf[off], &packed, 2);
            } else {
                gradient_buf[off + 0] = b;
                gradient_buf[off + 1] = g;
                gradient_buf[off + 2] = r;
                if (bpp == 4) gradient_buf[off + 3] = 0;
            }
        }
    }
}

void video_gradient_apply(void) {
    if (gradient_buf != NULL)
        memcpy(back, gradient_buf, gradient_buf_size);
}

void video_gradient_capture(void) {
    if (gradient_buf != NULL)
        memcpy(gradient_buf, back, gradient_buf_size);
}

void video_gradient_free(void) {
    if (gradient_buf != NULL) {
        free(gradient_buf);
        gradient_buf = NULL;
    }
}

int video_rounded_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t radius, uint32_t fill, uint32_t border) {
    if (x >= scr_w || y >= scr_h) return 1;
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;

    /* Clamp the corner radius so a clipped (partly off-screen) rect can never
     * make w-2*radius / h-2*radius underflow into a giant stretched fill. */
    if (2 * radius > w) radius = w / 2;
    if (2 * radius > h) radius = h / 2;

    video_rect(x + radius, y, w - 2 * radius, h, fill);
    video_rect(x, y + radius, radius, h - 2 * radius, fill);
    video_rect(x + w - radius, y + radius, radius, h - 2 * radius, fill);

    for (int dy = 0; dy <= (int)radius; dy++) {
        for (int dx = 0; dx <= (int)radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                video_pixel(x + radius - dx, y + radius - dy, fill);
                video_pixel(x + w - radius - 1 + dx, y + radius - dy, fill);
                video_pixel(x + radius - dx, y + h - radius - 1 + dy, fill);
                video_pixel(x + w - radius - 1 + dx, y + h - radius - 1 + dy, fill);
            }
        }
    }

    if (border != fill) {
        video_rect(x, y, w, 1, border);
        video_rect(x, y + h - 1, w, 1, border);
        video_rect(x, y, 1, h, border);
        video_rect(x + w - 1, y, 1, h, border);
    }
    return 0;
}

void video_flush_diff(void) {
    size_t total = (size_t) scr_w * scr_h * bpp;
    if (total == 0)
        return;

    /* No shadow (alloc failed) or first present: paint everything. */
    if (shadow == NULL || !shadow_valid) {
        memcpy(vram, back, total);
        if (shadow != NULL) {
            memcpy(shadow, back, total);
            shadow_valid = true;
        }
        return;
    }

    /* For each scanline: a fast (optimised) memcmp rejects the unchanged rows --
     * the vast majority on a static scene -- in cheap system RAM. Only when a row
     * differs do we scan it for the changed horizontal span and write just that
     * span to VRAM, so a moving 16-px cursor costs ~16 px of host-boundary
     * traffic, not a full 800-px row. */
    size_t row_bytes = (size_t) scr_w * bpp;
    for (unsigned y = 0; y < scr_h; y++) {
        size_t off = (size_t) y * row_bytes;
        if (memcmp(back + off, shadow + off, row_bytes) == 0)
            continue;

        size_t lo = 0;
        while (back[off + lo] == shadow[off + lo])
            lo++;
        size_t hi = row_bytes - 1;
        while (back[off + hi] == shadow[off + hi])
            hi--;

        /* Widen to whole-pixel boundaries (tidy, costs at most bpp-1 bytes). */
        lo -= lo % bpp;
        hi += (bpp - 1) - (hi % bpp);
        if (hi >= row_bytes)
            hi = row_bytes - 1;

        size_t span = hi - lo + 1;
        memcpy(vram   + off + lo, back + off + lo, span);
        memcpy(shadow + off + lo, back + off + lo, span);
    }
}
