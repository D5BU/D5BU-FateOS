#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

// Global mouse coordinates and physics state
extern int mx, my;
extern float rx, ry;
extern float ring_size;
extern float target_ring_size;
extern uint32_t ring_color;
extern int left_button_pressed;

// Keyboard character codes
#define KEY_BACKSPACE 127
#define KEY_ENTER     10
#define KEY_ESC       27
#define KEY_UP        1000
#define KEY_DOWN      1001
#define KEY_LEFT      1002
#define KEY_RIGHT     1003

// Input API
int init_input(void);
void close_input(void);
void poll_input(void);
int get_keyboard_char(void);
void render_cursor(void);

#endif // INPUT_H
