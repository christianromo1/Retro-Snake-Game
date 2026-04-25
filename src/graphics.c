#include <stdint.h>
#include "graphics.h"

static uint8_t  *fb_addr  = 0;
static uint32_t  fb_pitch = 0;
static uint32_t  fb_bpp   = 0;
int screen_width  = 0;
int screen_height = 0;

/* Minimal Multiboot2 tag structs for parsing */
struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint16_t reserved;
};

void graphics_init(uint32_t mb_info_addr) {
    /* First 8 bytes of MB2 info are total_size + reserved, skip them */
    struct mb2_tag *tag = (struct mb2_tag *)(mb_info_addr + 8);

    while (tag->type != 0) {
        if (tag->type == 8) {   /* type 8 = framebuffer info */
            struct mb2_tag_framebuffer *fb =
                (struct mb2_tag_framebuffer *)tag;
            fb_addr       = (uint8_t *)(uint32_t)fb->addr;
            fb_pitch      = fb->pitch;
            fb_bpp        = fb->bpp;
            screen_width  = (int)fb->width;
            screen_height = (int)fb->height;
            break;
        }
        /* Tags are 8-byte aligned */
        uint32_t next = (uint32_t)tag + tag->size;
        if (next & 7) next = (next + 7) & ~7u;
        tag = (struct mb2_tag *)next;
    }
}

void draw_pixel(int x, int y, uint32_t color) {
    if (!fb_addr || x < 0 || y < 0 ||
        x >= screen_width || y >= screen_height) return;
    uint32_t *pixel =
        (uint32_t *)(fb_addr + y * fb_pitch + x * (fb_bpp / 8));
    *pixel = color;
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            draw_pixel(col, row, color);
}

void clear_screen(uint32_t color) {
    draw_rect(0, 0, screen_width, screen_height, color);
}
