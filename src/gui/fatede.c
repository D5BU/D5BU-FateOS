#include "fatede.h"
#include "core.h"
#include "input.h"
#include "apps/apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static Window windows[MAX_WINDOWS];
static int window_count = 0;
static int active_win_id = -1;

// Drag state
static int dragging_win_id = -1;
static int drag_off_x = 0;
static int drag_off_y = 0;

// App Launcher Menu State
static int menu_open = 0;
#define MENU_W 200
#define MENU_H 180
static int menu_x1 = 15;
static int menu_y1 = 0; // calculated in loop (height - 40 - MENU_H)

Window *create_window(const char *title, int x, int y, int w, int h, 
                      void (*draw_content)(int, int, int, int, void*),
                      void (*handle_click)(int, int, int, void*),
                      void (*handle_key)(int, void*),
                      void *data) {
    if (window_count >= MAX_WINDOWS) return NULL;
    
    // Allocate ID
    static int next_id = 1;
    
    Window *win = &windows[window_count];
    win->id = next_id++;
    win->title = title;
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->is_minimized = 0;
    win->is_focused = 1;
    win->draw_content = draw_content;
    win->handle_click = handle_click;
    win->handle_key = handle_key;
    win->on_close = NULL;
    win->data = data;
    
    window_count++;
    focus_window(win->id);
    
    return win;
}

void destroy_window(int win_id) {
    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == win_id) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return;
    
    // Call close trigger
    if (windows[idx].on_close) {
        windows[idx].on_close(windows[idx].data);
    }
    
    // Shift windows down
    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    window_count--;
    
    // Focus next available window
    if (window_count > 0) {
        focus_window(windows[window_count - 1].id);
    } else {
        active_win_id = -1;
    }
}

void focus_window(int win_id) {
    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == win_id) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return;
    
    // Cache the window to focus
    Window temp = windows[idx];
    
    // Move to end of window array (so it renders on top)
    for (int i = idx; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    windows[window_count - 1] = temp;
    
    // Update focus state
    for (int i = 0; i < window_count; i++) {
        windows[i].is_focused = (i == window_count - 1);
    }
    active_win_id = win_id;
}

static void draw_window_container(Window *win) {
    if (win->is_minimized) return;
    
    // Outer border container
    uint32_t active_border = win->is_focused ? COLOR_TAUPE : COLOR_BORDER;
    float opacity = win->is_focused ? 0.35f : 0.15f;
    
    // Draw Window frame (glassmorphic dark look)
    draw_rect(win->x, win->y, win->x + win->w, win->y + win->h, COLOR_SURFACE, 0.92f);
    draw_border(win->x, win->y, win->x + win->w, win->y + win->h, active_border, opacity);
    
    // Title bar divider line
    draw_rect(win->x, win->y + 24, win->x + win->w, win->y + 25, COLOR_BORDER, 0.15f);
    
    // Title text
    uint32_t text_color = win->is_focused ? COLOR_TEXT : COLOR_MUTED;
    draw_string_bold(win->x + 12, win->y + 5, win->title, text_color);
    
    // Close button (red/coral dot on the top right)
    int cx = win->x + win->w - 15;
    int cy = win->y + 12;
    draw_circle(cx, cy, 6, win->is_focused ? COLOR_CORAL : COLOR_BORDER, 0.7f);
    
    // Content body drawing callback
    if (win->draw_content) {
        win->draw_content(win->x, win->y + 24, win->w, win->h - 24, win->data);
    }
}

void init_desktop() {
    printf("[-] Initializing FateDE Canvas...\n");
    window_count = 0;
    active_win_id = -1;
    dragging_win_id = -1;
    menu_open = 0;
    
    // Pre-spawn two windows to welcome user and show details
    spawn_system_monitor();
    spawn_text_editor();
}

void run_desktop_loop() {
    char clock_str[32] = "";
    char ip_str[32] = "OFFLINE";
    
    menu_y1 = height - 40 - MENU_H;
    
    // Scan network interface on launch
    FILE *net_fp = fopen("/sys/class/net/eth0/operstate", "r");
    if (!net_fp) net_fp = fopen("/sys/class/net/enp0s3/operstate", "r");
    if (net_fp) {
        char state[32];
        if (fgets(state, sizeof(state), net_fp)) {
            if (strncmp(state, "up", 2) == 0) {
                strcpy(ip_str, "CONNECTED");
            }
        }
        fclose(net_fp);
    }
    
    while (1) {
        poll_input();
        
        // --- 1. Interactive Mouse Hover Logic ---
        int is_hovering = 0;
        
        // Taskbar menu button hover (bottom-left)
        if (mx >= 15 && mx <= 95 && my >= height - 35 && my <= height - 5) {
            is_hovering = 1;
        }
        
        // Launcher Menu Hover
        if (menu_open) {
            if (mx >= menu_x1 && mx <= menu_x1 + MENU_W && my >= menu_y1 && my <= menu_y1 + MENU_H) {
                is_hovering = 1;
            }
        }
        
        // Windows titlebar hover / close button hover / content click checks
        int clicked_window_idx = -1;
        int clicked_close_btn = 0;
        int clicked_titlebar = 0;
        
        for (int i = window_count - 1; i >= 0; i--) {
            Window *win = &windows[i];
            if (win->is_minimized) continue;
            
            // Check if mouse falls inside window boundaries
            if (mx >= win->x && mx <= win->x + win->w && my >= win->y && my <= win->y + win->h) {
                is_hovering = 1;
                
                // Hover close button
                int cx = win->x + win->w - 15;
                int cy = win->y + 12;
                if ((mx - cx) * (mx - cx) + (my - cy) * (my - cy) <= 64) {
                    target_ring_size = 50.0f;
                    ring_color = COLOR_CORAL;
                }
                
                // Clicking logic
                if (left_button_pressed) {
                    clicked_window_idx = i;
                    // Click close button
                    if ((mx - cx) * (mx - cx) + (my - cy) * (my - cy) <= 64) {
                        clicked_close_btn = 1;
                    } 
                    // Click title bar
                    else if (my >= win->y && my <= win->y + 24) {
                        clicked_titlebar = 1;
                    }
                }
                break; // Topmost window intercepts input
            }
        }
        
        // --- 2. Handle Clicks ---
        if (left_button_pressed) {
            // Dismiss menu if clicking outside of it
            if (menu_open && !(mx >= menu_x1 && mx <= menu_x1 + MENU_W && my >= menu_y1 && my <= menu_y1 + MENU_H)) {
                menu_open = 0;
            }
            
            if (clicked_window_idx != -1) {
                Window *win = &windows[clicked_window_idx];
                focus_window(win->id);
                
                if (clicked_close_btn) {
                    destroy_window(win->id);
                } else if (clicked_titlebar) {
                    // Start dragging window
                    dragging_win_id = win->id;
                    drag_off_x = mx - win->x;
                    drag_off_y = my - win->y;
                } else {
                    // Send click to window content
                    if (win->handle_click) {
                        int local_x = mx - win->x;
                        int local_y = my - (win->y + 24);
                        win->handle_click(local_x, local_y, 1, win->data);
                    }
                }
            } 
            // Click menu button
            else if (mx >= 15 && mx <= 95 && my >= height - 35 && my <= height - 5) {
                menu_open = 1 - menu_open;
            }
            // Click inside active app launcher menu
            else if (menu_open && mx >= menu_x1 && mx <= menu_x1 + MENU_W && my >= menu_y1 && my <= menu_y1 + MENU_H) {
                int item_idx = (my - menu_y1 - 10) / 25;
                if (item_idx == 0) spawn_system_monitor();
                else if (item_idx == 1) spawn_text_editor();
                else if (item_idx == 2) spawn_file_manager();
                else if (item_idx == 3) spawn_terminal();
                else if (item_idx == 4) spawn_calculator();
                else if (item_idx == 5) spawn_invaders_game();
                menu_open = 0; // Auto-close
            }
            
            left_button_pressed = 0; // Release
        }
        
        // --- 3. Dragging Mechanics ---
        if (dragging_win_id != -1) {
            // Check if mouse is still held down, otherwise release dragging
            // Since poll_input queries mice relative movement, button released will clear left_button_pressed.
            // Wait, we need to track if button is still held down. The relative driver in `/dev/input/mice` 
            // keeps sending packets with left_button bit set if held. If not, it sends packet with bit cleared.
            // Wait, if no packet is read, left_button_pressed might remain 1 unless we clear it or track real state.
            // In input.c, poll_input writes `left_button_pressed = mouse_data[0] & 0x1;` which is perfect!
            // It updates the state every frame. If it is 0, dragging stops.
            // Let's check:
            // Let's see: input.c poll_input updates left_button_pressed. If it's 0, stop dragging:

            // But we reset it in handles! Oh! If we reset it inside handles, then the next poll will update it correctly.
            // However, to keep dragging smooth, dragging should look at input state:
            // Let's look at `extern int left_button_pressed;`. If poll_input sets it to 0 when released, we handle it:
            // Actually, in input.c: `left_button_pressed = mouse_data[0] & 0x1;` on successful read.
            // So if left button is released, the next mouse update will set it to 0.
            // Let's modify our dragging check to check `left_button_pressed`:
            if (left_button_pressed) {
                // Find dragging window
                Window *win = NULL;
                for (int i = 0; i < window_count; i++) {
                    if (windows[i].id == dragging_win_id) {
                        win = &windows[i];
                        break;
                    }
                }
                if (win) {
                    win->x = mx - drag_off_x;
                    win->y = my - drag_off_y;
                    
                    // Clamp windows from flying off screen
                    if (win->y < 0) win->y = 0;
                    if (win->y > height - 40) win->y = height - 40;
                }
            } else {
                dragging_win_id = -1;
            }
        }
        
        // --- 4. Route Keyboard Input to Focused Window ---
        int key = get_keyboard_char();
        if (key != -1) {
            // Find focused window
            Window *focused_win = NULL;
            for (int i = 0; i < window_count; i++) {
                if (windows[i].id == active_win_id) {
                    focused_win = &windows[i];
                    break;
                }
            }
            if (focused_win && focused_win->handle_key) {
                focused_win->handle_key(key, focused_win->data);
            }
        }
        
        // --- 5. Render Canvas Frame ---
        clear_screen();
        draw_desktop_background();
        
        // Render inactive/active windows (Z-order: 0 is bottom-most, window_count-1 is top-most)
        for (int i = 0; i < window_count; i++) {
            draw_window_container(&windows[i]);
        }
        
        // App Launcher Menu overlay
        if (menu_open) {
            draw_rect(menu_x1, menu_y1, menu_x1 + MENU_W, menu_y1 + MENU_H, COLOR_SURFACE, 0.95f);
            draw_border(menu_x1, menu_y1, menu_x1 + MENU_W, menu_y1 + MENU_H, COLOR_TAUPE, 0.4f);
            
            const char *menu_items[] = {
                "1. System Monitor",
                "2. Text Editor",
                "3. File Manager",
                "4. Terminal Console",
                "5. Calculator",
                "6. Retro Invaders"
            };
            
            for (int i = 0; i < 6; i++) {
                int item_y = menu_y1 + 10 + i * 25;
                // Highlight item if mouse is hovering over it
                if (mx >= menu_x1 + 5 && mx <= menu_x1 + MENU_W - 5 && my >= item_y && my <= item_y + 22) {
                    draw_rect(menu_x1 + 5, item_y, menu_x1 + MENU_W - 5, item_y + 22, COLOR_BG, 1.0f);
                    draw_string(menu_x1 + 15, item_y + 3, menu_items[i], COLOR_CORAL);
                } else {
                    draw_string(menu_x1 + 15, item_y + 3, menu_items[i], COLOR_TEXT);
                }
            }
        }
        
        // --- 6. Taskbar / Panel rendering ---
        int panel_y1 = height - 40;
        int panel_y2 = height - 1;
        draw_rect(0, panel_y1, width - 1, panel_y2, COLOR_SURFACE, 0.96f);
        draw_border(0, panel_y1, width - 1, panel_y2, COLOR_BORDER, 0.15f);
        
        // Menu pill button "FateOS"
        int mbtn_x1 = 15;
        int mbtn_y1 = panel_y1 + 5;
        int mbtn_x2 = 95;
        int mbtn_y2 = panel_y2 - 5;
        uint32_t mbtn_bg = menu_open ? COLOR_CORAL : COLOR_SURFACE;
        uint32_t mbtn_txt = menu_open ? 0x000000 : COLOR_TAUPE;
        draw_rect(mbtn_x1, mbtn_y1, mbtn_x2, mbtn_y2, mbtn_bg, 1.0f);
        draw_border(mbtn_x1, mbtn_y1, mbtn_x2, mbtn_y2, COLOR_TAUPE, 0.5f);
        draw_string_bold(mbtn_x1 + 16, mbtn_y1 + 8, "FateOS", mbtn_txt);
        
        // Active Window List Tabs in center taskbar
        int tab_start_x = 110;
        for (int i = 0; i < window_count; i++) {
            Window *win = &windows[i];
            int tab_w = 90;
            int tx1 = tab_start_x + i * (tab_w + 5);
            int ty1 = panel_y1 + 5;
            int tx2 = tx1 + tab_w;
            int ty2 = panel_y2 - 5;
            
            uint32_t tab_bg = win->is_focused ? COLOR_BG : COLOR_SURFACE;
            uint32_t tab_txt = win->is_focused ? COLOR_CORAL : COLOR_MUTED;
            draw_rect(tx1, ty1, tx2, ty2, tab_bg, 1.0f);
            draw_border(tx1, ty1, tx2, ty2, COLOR_BORDER, win->is_focused ? 0.4f : 0.15f);
            
            // Truncate title for tab
            char tab_title[10];
            strncpy(tab_title, win->title, 8);
            tab_title[8] = '\0';
            draw_string(tx1 + 8, ty1 + 8, tab_title, tab_txt);
        }
        
        // Right section: IP and time clock
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        if (timeinfo) {
            sprintf(clock_str, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        }
        
        draw_string(width - 240, panel_y1 + 13, "NET:", COLOR_MUTED);
        draw_string_bold(width - 200, panel_y1 + 12, ip_str, (strcmp(ip_str, "OFFLINE") == 0) ? COLOR_CORAL : COLOR_TAUPE);
        
        draw_string(width - 95, panel_y1 + 13, clock_str, COLOR_TEXT);
        
        // Signature Portfolio Cursor expansion & color shifts
        if (is_hovering) {
            target_ring_size = 56.0f;
            ring_color = COLOR_CORAL;
        } else {
            target_ring_size = 40.0f;
            ring_color = COLOR_TAUPE;
        }
        
        // Cursor drawing
        render_cursor();
        
        blit_screen();
        usleep(16666);
    }
}
