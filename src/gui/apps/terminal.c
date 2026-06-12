#include "../fatede.h"
#include "../core.h"
#include "../input.h"
#include "apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERM_ROWS 10
#define TERM_COLS 50

typedef struct {
    char history[TERM_ROWS][TERM_COLS];
    int row_count;
    char cmd_buffer[64];
    int cmd_len;
} TermState;

static void append_history(TermState *state, const char *line) {
    if (state->row_count < TERM_ROWS) {
        strncpy(state->history[state->row_count], line, TERM_COLS - 1);
        state->history[state->row_count][TERM_COLS - 1] = '\0';
        state->row_count++;
    } else {
        // Shift history up
        for (int r = 1; r < TERM_ROWS; r++) {
            strcpy(state->history[r - 1], state->history[r]);
        }
        strncpy(state->history[TERM_ROWS - 1], line, TERM_COLS - 1);
        state->history[TERM_ROWS - 1][TERM_COLS - 1] = '\0';
    }
}

static void execute_command(TermState *state) {
    char display_cmd[128];
    sprintf(display_cmd, "~ # %s", state->cmd_buffer);
    append_history(state, display_cmd);
    
    // Command overrides
    if (strcmp(state->cmd_buffer, "clear") == 0) {
        state->row_count = 0;
        state->cmd_buffer[0] = '\0';
        state->cmd_len = 0;
        return;
    }
    
    // Execute command via popen
    FILE *fp = popen(state->cmd_buffer, "r");
    if (fp) {
        char line_buf[128];
        int lines_read = 0;
        while (fgets(line_buf, sizeof(line_buf), fp) && lines_read < 8) {
            // Strip trailing newline
            int len = strlen(line_buf);
            if (len > 0 && line_buf[len - 1] == '\n') {
                line_buf[len - 1] = '\0';
            }
            append_history(state, line_buf);
            lines_read++;
        }
        pclose(fp);
        if (lines_read == 0) {
            append_history(state, "Command executed with no output.");
        }
    } else {
        append_history(state, "Error running command!");
    }
    
    state->cmd_buffer[0] = '\0';
    state->cmd_len = 0;
}

static void draw_terminal(int x, int y, int w, int h, void *data) {
    TermState *state = (TermState *)data;
    
    // Terminal background
    draw_rect(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BG, 1.0f);
    draw_border(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BORDER, 0.2f);
    
    int start_x = x + 18;
    int start_y = y + 18;
    
    // Render command history
    for (int r = 0; r < state->row_count; r++) {
        draw_string(start_x, start_y + r * 16, state->history[r], COLOR_TEXT);
    }
    
    // Render current active command line prompt
    int input_y = start_y + state->row_count * 16;
    if (input_y < y + h - 25) {
        draw_string(start_x, input_y, "~ # ", COLOR_CORAL);
        draw_string(start_x + 32, input_y, state->cmd_buffer, COLOR_TEXT);
        
        // Draw standard terminal cursor block
        int cursor_x = start_x + 32 + state->cmd_len * 8;
        draw_rect(cursor_x, input_y + 2, cursor_x + 8, input_y + 16, COLOR_TAUPE, 0.6f);
    }
}

static void handle_key_terminal(int key, void *data) {
    TermState *state = (TermState *)data;
    
    if (key == KEY_BACKSPACE) {
        if (state->cmd_len > 0) {
            state->cmd_len--;
            state->cmd_buffer[state->cmd_len] = '\0';
        }
    } else if (key == KEY_ENTER) {
        if (state->cmd_len > 0) {
            execute_command(state);
        } else {
            append_history(state, "~ # ");
        }
    } else if (key >= 32 && key <= 126) {
        if (state->cmd_len < 60) {
            state->cmd_buffer[state->cmd_len++] = (char)key;
            state->cmd_buffer[state->cmd_len] = '\0';
        }
    }
}

static void on_close_terminal(void *data) {
    if (data) free(data);
}

void spawn_terminal() {
    TermState *state = (TermState *)malloc(sizeof(TermState));
    if (!state) return;
    memset(state, 0, sizeof(TermState));
    
    // Populate introductory text
    append_history(state, "Loom Visual Shell - FateOS V1");
    append_history(state, "Type 'ls', 'free', 'uname -a', or 'clear'.");
    append_history(state, "-----------------------------");
    
    int win_w = 400;
    int win_h = 230;
    int win_x = (width - win_w) / 2 + 30;
    int win_y = (height - win_h) / 2 + 50;
    
    Window *win = create_window("Loom Terminal", win_x, win_y, win_w, win_h, 
                                draw_terminal, NULL, handle_key_terminal, state);
    if (win) {
        win->on_close = on_close_terminal;
    }
}
