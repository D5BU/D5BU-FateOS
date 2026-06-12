#include "../fatede.h"
#include "../core.h"
#include "apps.h"
#include "../input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIEN_ROWS 3
#define ALIEN_COLS 5
#define GAME_W     300
#define GAME_H     200

typedef struct {
    int state; // 0 = Start, 1 = Playing, 2 = Game Over, 3 = Won
    int player_x;
    int laser_x, laser_y;
    int laser_active;
    
    int alien_x, alien_y;
    int alien_dir; // 1 = right, -1 = left
    int aliens[ALIEN_ROWS][ALIEN_COLS];
    
    int score;
    int tick_count;
    int move_interval;
} GameState;

static void init_game_state(GameState *state) {
    state->state = 0;
    state->player_x = GAME_W / 2;
    state->laser_active = 0;
    state->alien_x = 20;
    state->alien_y = 30;
    state->alien_dir = 1;
    state->score = 0;
    state->tick_count = 0;
    state->move_interval = 40; // update speed
    
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            state->aliens[r][c] = 1;
        }
    }
}

static void update_game_physics(GameState *state) {
    if (state->state != 1) return;
    
    state->tick_count++;
    
    // 1. Update Laser
    if (state->laser_active) {
        state->laser_y -= 5;
        if (state->laser_y < 25) {
            state->laser_active = 0;
        } else {
            // Collision check
            for (int r = 0; r < ALIEN_ROWS; r++) {
                for (int c = 0; c < ALIEN_COLS; c++) {
                    if (state->aliens[r][c]) {
                        int ax = state->alien_x + c * 25;
                        int ay = state->alien_y + r * 20;
                        if (state->laser_x >= ax && state->laser_x <= ax + 16 &&
                            state->laser_y >= ay && state->laser_y <= ay + 12) {
                            state->aliens[r][c] = 0;
                            state->laser_active = 0;
                            state->score += 20;
                            
                            // Check win
                            int remaining = 0;
                            for (int r2 = 0; r2 < ALIEN_ROWS; r2++) {
                                for (int c2 = 0; c2 < ALIEN_COLS; c2++) {
                                    if (state->aliens[r2][c2]) remaining++;
                                }
                            }
                            if (remaining == 0) {
                                state->state = 3; // Won!
                            }
                            break;
                        }
                    }
                }
                if (!state->laser_active) break;
            }
        }
    }
    
    // 2. Update Aliens
    if (state->tick_count >= state->move_interval) {
        state->tick_count = 0;
        
        // Find grid boundaries
        int min_c = ALIEN_COLS, max_c = -1;
        int max_r = -1;
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                if (state->aliens[r][c]) {
                    if (c < min_c) min_c = c;
                    if (c > max_c) max_c = c;
                    if (r > max_r) max_r = r;
                }
            }
        }
        
        if (max_c == -1) {
            state->state = 3; // Win condition fallback
            return;
        }
        
        int left_edge = state->alien_x + min_c * 25;
        int right_edge = state->alien_x + max_c * 25 + 16;
        
        int shift_down = 0;
        if (state->alien_dir == 1 && right_edge >= GAME_W - 20) {
            state->alien_dir = -1;
            shift_down = 1;
        } else if (state->alien_dir == -1 && left_edge <= 20) {
            state->alien_dir = 1;
            shift_down = 1;
        }
        
        if (shift_down) {
            state->alien_y += 10;
            // Check fail height
            int bottom_edge = state->alien_y + max_r * 20 + 12;
            if (bottom_edge >= GAME_H - 35) {
                state->state = 2; // Game Over
            }
        } else {
            state->alien_x += state->alien_dir * 8;
        }
    }
}

static void draw_game(int x, int y, int w, int h, void *data) {
    GameState *state = (GameState *)data;
    
    update_game_physics(state);
    
    // Game Canvas
    draw_rect(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BG, 1.0f);
    draw_border(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BORDER, 0.2f);
    
    int gx = x + 10;
    int gy = y + 10;
    
    if (state->state == 0) {
        // Start Screen
        draw_string_bold(gx + 60, gy + 50, "FATE INVADERS", COLOR_CORAL);
        draw_string(gx + 40, gy + 90, "CLICK TO START PORTAL", COLOR_TEXT);
        draw_string(gx + 30, gy + 120, "ARROWS TO MOVE, SPACE TO SHOOT", COLOR_MUTED);
    } else if (state->state == 2) {
        // Game Over
        draw_string_bold(gx + 80, gy + 60, "PORTAL CLOSED", COLOR_CORAL);
        draw_string(gx + 65, gy + 90, "CREDITS DEPLETED", COLOR_MUTED);
        draw_string(gx + 45, gy + 130, "CLICK TO RE-TRY ACCESS", COLOR_TEXT);
    } else if (state->state == 3) {
        // Game Won
        draw_string_bold(gx + 85, gy + 60, "ACCESS GRANTED", COLOR_TAUPE);
        char score_str[32];
        sprintf(score_str, "FINAL SCORE: %d", state->score);
        draw_string(gx + 60, gy + 90, score_str, COLOR_TEXT);
        draw_string(gx + 45, gy + 130, "CLICK FOR NEW CHALLENGE", COLOR_TAUPE);
    } else if (state->state == 1) {
        // Playing
        // Draw Score
        char score_txt[32];
        sprintf(score_txt, "SCORE: %d", state->score);
        draw_string(gx + 10, gy + 8, score_txt, COLOR_TAUPE);
        
        // Draw player ship (triangle simulator)
        int px = gx + state->player_x;
        int py = gy + GAME_H - 25;
        // Draw a simple 16x10 block/triangle
        draw_rect(px - 8, py, px + 8, py + 6, COLOR_TAUPE, 1.0f);
        draw_rect(px - 2, py - 4, px + 2, py, COLOR_TAUPE, 1.0f);
        
        // Draw laser
        if (state->laser_active) {
            draw_rect(gx + state->laser_x, gy + state->laser_y, gx + state->laser_x + 1, gy + state->laser_y + 6, COLOR_CORAL, 1.0f);
        }
        
        // Draw aliens
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS; c++) {
                if (state->aliens[r][c]) {
                    int ax = gx + state->alien_x + c * 25;
                    int ay = gy + state->alien_y + r * 20;
                    
                    // Render simple 12x8 pixel block as invader
                    draw_rect(ax, ay, ax + 12, ay + 8, COLOR_TEXT, 1.0f);
                    // Draw tiny alien antenna eyes
                    put_pixel(ax + 2, ay - 1, COLOR_CORAL);
                    put_pixel(ax + 9, ay - 1, COLOR_CORAL);
                }
            }
        }
    }
}

static void handle_click_game(int local_x, int local_y, int button, void *data) {
    GameState *state = (GameState *)data;
    if (state->state != 1) {
        // Reset and start
        init_game_state(state);
        state->state = 1;
    }
}

static void handle_key_game(int key, void *data) {
    GameState *state = (GameState *)data;
    if (state->state != 1) return;
    
    if (key == KEY_LEFT) {
        state->player_x -= 12;
        if (state->player_x < 15) state->player_x = 15;
    } else if (key == KEY_RIGHT) {
        state->player_x += 12;
        if (state->player_x > GAME_W - 15) state->player_x = GAME_W - 15;
    } else if (key == ' ' || key == KEY_UP) {
        if (!state->laser_active) {
            state->laser_active = 1;
            state->laser_x = state->player_x;
            state->laser_y = GAME_H - 30;
        }
    }
}

static void on_close_game(void *data) {
    if (data) free(data);
}

void spawn_invaders_game() {
    GameState *state = (GameState *)malloc(sizeof(GameState));
    if (!state) return;
    memset(state, 0, sizeof(GameState));
    
    init_game_state(state);
    
    int win_w = 320;
    int win_h = 220;
    int win_x = (width - win_w) / 2 + 100;
    int win_y = (height - win_h) / 2 + 20;
    
    Window *win = create_window("Fate Invaders", win_x, win_y, win_w, win_h, 
                                draw_game, handle_click_game, handle_key_game, state);
    if (win) {
        win->on_close = on_close_game;
    }
}
