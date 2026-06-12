#include "../fatede.h"
#include "../core.h"
#include "apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char display[32];
    double accumulator;
    char pending_op;
    int clear_display;
} CalcState;

static void draw_calc(int x, int y, int w, int h, void *data) {
    CalcState *state = (CalcState *)data;
    
    // Background
    draw_rect(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BG, 1.0f);
    draw_border(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BORDER, 0.2f);
    
    // Display Box
    int disp_x1 = x + 18, disp_x2 = x + w - 18;
    int disp_y1 = y + 18, disp_y2 = y + 50;
    draw_rect(disp_x1, disp_y1, disp_x2, disp_y2, COLOR_SURFACE, 1.0f);
    draw_border(disp_x1, disp_y1, disp_x2, disp_y2, COLOR_BORDER, 0.4f);
    
    // Right align display text (each char is 8px wide)
    int text_len = strlen(state->display);
    int text_x = disp_x2 - 12 - text_len * 8;
    if (text_x < disp_x1 + 10) text_x = disp_x1 + 10;
    draw_string_bold(text_x, disp_y1 + 10, state->display, COLOR_TEXT);
    
    // Grid coordinates
    int btn_w = 40;
    int btn_h = 32;
    int start_x = x + 18;
    int start_y = y + 60;
    
    const char *buttons[5][4] = {
        {"C", "<-", "", "/"},
        {"7", "8", "9", "*"},
        {"4", "5", "6", "-"},
        {"1", "2", "3", "+"},
        {"0", ".", "=", ""}
    };
    
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (strcmp(buttons[r][c], "") == 0) continue;
            
            // Adjust equal button width
            int bw = btn_w;
            if (r == 4 && c == 2) bw = btn_w * 2 + 6; // Merge '=' and space
            
            int bx1 = start_x + c * (btn_w + 6);
            int by1 = start_y + r * (btn_h + 6);
            int bx2 = bx1 + bw;
            int by2 = by1 + btn_h;
            
            draw_rect(bx1, by1, bx2, by2, COLOR_SURFACE, 1.0f);
            
            // Highlight operators differently
            uint32_t border_color = COLOR_BORDER;
            const char *label = buttons[r][c];
            if (strcmp(label, "+") == 0 || strcmp(label, "-") == 0 || 
                strcmp(label, "*") == 0 || strcmp(label, "/") == 0 || strcmp(label, "=") == 0) {
                border_color = COLOR_CORAL;
            } else if (strcmp(label, "C") == 0) {
                border_color = COLOR_TAUPE;
            }
            
            draw_border(bx1, by1, bx2, by2, border_color, 0.4f);
            
            // Center the label text
            int label_len = strlen(label);
            int lx = bx1 + (bw - label_len * 8) / 2;
            int ly = by1 + (btn_h - 16) / 2;
            draw_string_bold(lx, ly, label, COLOR_TEXT);
        }
    }
}

static void calculate(CalcState *state, double value) {
    switch (state->pending_op) {
        case '+': state->accumulator += value; break;
        case '-': state->accumulator -= value; break;
        case '*': state->accumulator *= value; break;
        case '/': 
            if (value != 0.0) state->accumulator /= value; 
            else strcpy(state->display, "DIV BY ZERO");
            break;
        default: state->accumulator = value; break;
    }
}

static void handle_click_calc(int local_x, int local_y, int button, void *data) {
    CalcState *state = (CalcState *)data;
    
    int start_x = 18;
    int start_y = 60;
    int btn_w = 40;
    int btn_h = 32;
    
    const char *buttons[5][4] = {
        {"C", "<-", "", "/"},
        {"7", "8", "9", "*"},
        {"4", "5", "6", "-"},
        {"1", "2", "3", "+"},
        {"0", ".", "=", ""}
    };
    
    // Check which grid box was clicked
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (strcmp(buttons[r][c], "") == 0) continue;
            
            int bw = btn_w;
            if (r == 4 && c == 2) bw = btn_w * 2 + 6;
            
            int bx1 = start_x + c * (btn_w + 6);
            int by1 = start_y + r * (btn_h + 6);
            int bx2 = bx1 + bw;
            int by2 = by1 + btn_h;
            
            if (local_x >= bx1 && local_x <= bx2 && local_y >= by1 && local_y <= by2) {
                const char *label = buttons[r][c];
                
                if (label[0] >= '0' && label[0] <= '9') {
                    if (state->clear_display || strcmp(state->display, "0") == 0) {
                        strcpy(state->display, label);
                        state->clear_display = 0;
                    } else {
                        if (strlen(state->display) < 16) strcat(state->display, label);
                    }
                } else if (strcmp(label, ".") == 0) {
                    if (state->clear_display) {
                        strcpy(state->display, "0.");
                        state->clear_display = 0;
                    } else if (strchr(state->display, '.') == NULL) {
                        strcat(state->display, ".");
                    }
                } else if (strcmp(label, "C") == 0) {
                    strcpy(state->display, "0");
                    state->accumulator = 0.0;
                    state->pending_op = ' ';
                    state->clear_display = 0;
                } else if (strcmp(label, "<-") == 0) {
                    int len = strlen(state->display);
                    if (len > 1) {
                        state->display[len - 1] = '\0';
                    } else {
                        strcpy(state->display, "0");
                    }
                } else if (strcmp(label, "=") == 0) {
                    double val = atof(state->display);
                    calculate(state, val);
                    sprintf(state->display, "%g", state->accumulator);
                    state->pending_op = ' ';
                    state->clear_display = 1;
                } else {
                    // Operator clicked (+, -, *, /)
                    double val = atof(state->display);
                    if (state->pending_op != ' ') {
                        calculate(state, val);
                    } else {
                        state->accumulator = val;
                    }
                    state->pending_op = label[0];
                    sprintf(state->display, "%g", state->accumulator);
                    state->clear_display = 1;
                }
                return;
            }
        }
    }
}

static void on_close_calc(void *data) {
    if (data) free(data);
}

void spawn_calculator() {
    CalcState *state = (CalcState *)malloc(sizeof(CalcState));
    if (!state) return;
    memset(state, 0, sizeof(CalcState));
    
    strcpy(state->display, "0");
    state->pending_op = ' ';
    state->clear_display = 1;
    
    int win_w = 216;
    int win_h = 270;
    int win_x = (width - win_w) / 2 - 180;
    int win_y = (height - win_h) / 2 + 40;
    
    Window *win = create_window("FateCalc", win_x, win_y, win_w, win_h, 
                                draw_calc, handle_click_calc, NULL, state);
    if (win) {
        win->on_close = on_close_calc;
    }
}
