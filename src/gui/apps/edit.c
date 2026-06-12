#include "../fatede.h"
#include "../core.h"
#include "../input.h"
#include "apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EDIT_BUFFER_SIZE 2048

typedef struct {
    char buffer[EDIT_BUFFER_SIZE];
    int cursor_pos;
    char status_message[64];
    int status_timer;
} EditState;

static void draw_editor(int x, int y, int w, int h, void *data) {
    EditState *state = (EditState *)data;
    
    // Editor background canvas
    draw_rect(x + 10, y + 40, x + w - 10, y + h - 10, COLOR_BG, 1.0f);
    draw_border(x + 10, y + 40, x + w - 10, y + h - 10, COLOR_BORDER, 0.2f);
    
    // Save Button at the top bar
    int btn_x1 = x + w - 80;
    int btn_y1 = y + 10;
    int btn_x2 = x + w - 15;
    int btn_y2 = y + 32;
    draw_rect(btn_x1, btn_y1, btn_x2, btn_y2, COLOR_SURFACE, 1.0f);
    draw_border(btn_x1, btn_y1, btn_x2, btn_y2, COLOR_TAUPE, 0.5f);
    draw_string(btn_x1 + 12, btn_y1 + 5, "SAVE", COLOR_TAUPE);

    // Render text with line breaks
    int start_x = x + 18;
    int start_y = y + 48;
    int cur_x = start_x;
    int cur_y = start_y;
    
    for (int i = 0; i < state->cursor_pos; i++) {
        char c = state->buffer[i];
        if (c == '\n') {
            cur_x = start_x;
            cur_y += 18;
        } else {
            draw_char(cur_x, cur_y, c, COLOR_TEXT);
            cur_x += 8;
            if (cur_x >= x + w - 24) {
                cur_x = start_x;
                cur_y += 18;
            }
        }
    }
    
    // Draw cursor block (blinking simulator - always on for simplicity)
    draw_rect(cur_x, cur_y + 2, cur_x + 8, cur_y + 16, COLOR_TAUPE, 0.5f);

    // Render status message
    if (state->status_timer > 0) {
        draw_string(x + 15, y + 15, state->status_message, COLOR_CORAL);
        state->status_timer--;
    } else {
        draw_string(x + 15, y + 15, "TYPE TEXT INSIDE THE PAD", COLOR_MUTED);
    }
}

static void handle_click_editor(int local_x, int local_y, int button, void *data) {
    EditState *state = (EditState *)data;

    
    // Wait, the context of handle_click passes local coordinate (mouse coordinates offset by win->x, win->y)
    // Find absolute width of window to compute Save button click
    // Note that the parameters pass local_x, local_y relative to window x, y
    // Save button local coords: x + w - 80 is local_x = w - 80 to w - 15, local_y = 10 to 32
    // Let's assume window width is win->w (passed in state, wait, state doesn't have win width unless we know it.
    // The window width is 380 by default. So save button is local_x = 300 to 365, local_y = 10 to 32. Let's make it robust!
    // Let's check local_x, local_y. If local_y >= 10 && local_y <= 32 && local_x >= 300 && local_x <= 365:
    if (local_y >= 10 && local_y <= 32 && local_x >= 300) {
        // Save text to /tmp/notes.txt
        FILE *fp = fopen("/tmp/notes.txt", "w");
        if (fp) {
            fwrite(state->buffer, 1, state->cursor_pos, fp);
            fclose(fp);
            strcpy(state->status_message, "SAVED TO /tmp/notes.txt");
        } else {
            strcpy(state->status_message, "SAVE FAILED!");
        }
        state->status_timer = 90; // ~1.5s
    }
}

static void handle_key_editor(int key, void *data) {
    EditState *state = (EditState *)data;
    
    if (key == KEY_BACKSPACE) {
        if (state->cursor_pos > 0) {
            state->cursor_pos--;
            state->buffer[state->cursor_pos] = '\0';
        }
    } else if (key == KEY_ENTER) {
        if (state->cursor_pos < EDIT_BUFFER_SIZE - 1) {
            state->buffer[state->cursor_pos++] = '\n';
            state->buffer[state->cursor_pos] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (state->cursor_pos < EDIT_BUFFER_SIZE - 1) {
            state->buffer[state->cursor_pos++] = (char)key;
            state->buffer[state->cursor_pos] = '\0';
        }
    }
}

static void on_close_editor(void *data) {
    if (data) free(data);
}

void spawn_text_editor() {
    EditState *state = (EditState *)malloc(sizeof(EditState));
    if (!state) return;
    memset(state, 0, sizeof(EditState));
    
    // Load pre-existing notes if they exist
    FILE *fp = fopen("/tmp/notes.txt", "r");
    if (fp) {
        state->cursor_pos = fread(state->buffer, 1, EDIT_BUFFER_SIZE - 1, fp);
        state->buffer[state->cursor_pos] = '\0';
        fclose(fp);
    } else {
        strcpy(state->buffer, "Hello! Type anything here...\n\n");
        state->cursor_pos = strlen(state->buffer);
    }
    
    int win_w = 380;
    int win_h = 240;
    int win_x = (width - win_w) / 2 + 10;
    int win_y = (height - win_h) / 2 - 30;
    
    Window *win = create_window("Loom Edit", win_x, win_y, win_w, win_h, 
                                draw_editor, handle_click_editor, handle_key_editor, state);
    if (win) {
        win->on_close = on_close_editor;
    }
}
