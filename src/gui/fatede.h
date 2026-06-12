#ifndef FATEDE_H
#define FATEDE_H

#include <stdint.h>

#define MAX_WINDOWS 8

// Window structure representing an interactive GUI window container
typedef struct {
    int id;
    const char *title;
    int x, y;
    int w, h;
    int is_minimized;
    int is_focused;
    
    // Callback functions for app logic
    void (*draw_content)(int x, int y, int w, int h, void *data);
    void (*handle_click)(int local_x, int local_y, int button, void *data);
    void (*handle_key)(int key, void *data);
    void (*on_close)(void *data);
    
    void *data; // App-specific context state
} Window;

// Desktop Environment API
void init_desktop(void);
void run_desktop_loop(void);

// Window Manager helpers
Window *create_window(const char *title, int x, int y, int w, int h, 
                      void (*draw_content)(int, int, int, int, void*),
                      void (*handle_click)(int, int, int, void*),
                      void (*handle_key)(int, void*),
                      void *data);
void destroy_window(int win_id);
void focus_window(int win_id);

#endif // FATEDE_H
