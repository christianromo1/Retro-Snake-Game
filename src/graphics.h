#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <stdint.h>

void graphics_init(uint32_t mb_info_addr);
void draw_pixel(int x, int y, uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void clear_screen(uint32_t color);

extern int screen_width;
extern int screen_height;

#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_GREEN   0x0000AA00
#define COLOR_RED     0x00CC0000
#define COLOR_YELLOW  0x00CCCC00
#define COLOR_GRAY    0x00444444
#define COLOR_DKGREEN 0x00005500

void bitBlit(int dest_x, int dest_y,
             const unsigned char *pixel_data,
             int sprite_w, int sprite_h);

#endif
