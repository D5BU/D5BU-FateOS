#ifndef CORE_H
#define CORE_H

#include <stdint.h>

// Screen dimensions (will be initialized from FB info)
extern int width;
extern int height;
extern int line_len;

// Colors matching the editorial portfolio design
#define COLOR_BG      0x0d0d0d  // Deep charcoal background
#define COLOR_SURFACE 0x1a1a1a  // Translucent surface panel
#define COLOR_TAUPE   0xb7ab98  // Primary Accent: Warm Taupe
#define COLOR_CORAL   0xeb5939  // Secondary Accent: Coral Orange
#define COLOR_TEXT    0xe2e2e8  // Satoshi Primary Text
#define COLOR_MUTED   0xa0a0a0  // Satoshi Body Text (Muted gray)
#define COLOR_BORDER  0x767676  // Muted gray for window borders

// Core Graphics Functions
int init_graphics(void);
void close_graphics(void);
void blit_screen(void);
void clear_screen(void);

// Pixel and Color Operations
void put_pixel(int x, int y, uint32_t color);
uint32_t get_pixel(int x, int y);
uint32_t blend_color(uint32_t base, uint32_t blend, float alpha);

// Drawing Shapes
void draw_rect(int x1, int y1, int x2, int y2, uint32_t color, float alpha);
void draw_border(int x1, int y1, int x2, int y2, uint32_t color, float opacity);
void draw_circle(int cx, int cy, int r, uint32_t color, float alpha);
void draw_circle_outline(int cx, int cy, int r, uint32_t color, float opacity);

// Text Rendering
void draw_char(int x, int y, char c, uint32_t color);
void draw_string(int x, int y, const char *str, uint32_t color);
void draw_string_bold(int x, int y, const char *str, uint32_t color);
void draw_string_large(int x, int y, const char *str, uint32_t color);

// Desktop Background
void draw_desktop_background(void);

#endif // CORE_H
