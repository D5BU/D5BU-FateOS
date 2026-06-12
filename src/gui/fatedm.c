#include "fatedm.h"
#include "core.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


int run_login_screen() {
    char username[32] = "";
    char password[32] = "";
    int active_field = 0; // 0 = username, 1 = password
    char error_msg[64] = "";
    int error_timer = 0;
    
    // Login card coordinates
    int card_w = 420;
    int card_h = 320;
    int card_x1 = (width - card_w) / 2;
    int card_y1 = (height - card_h) / 2;
    int card_x2 = card_x1 + card_w;
    int card_y2 = card_y1 + card_h;

    // Boot Intro Animation (Sleek loading sequence)
    printf("[-] Running boot intro animation...\n");
    for (int frame = 0; frame < 90; frame++) {
        poll_input();
        clear_screen();
        draw_desktop_background();
        
        // Draw centered loading ring
        int cx = width / 2;
        int cy = height / 2;
        float progress_angle = (frame * 4.0f) * M_PI / 180.0f;
        
        // Spinning dots
        for (int i = 0; i < 8; i++) {
            float angle = progress_angle + (i * M_PI / 4.0f);
            int dx = (int)(cosf(angle) * 30.0f);
            int dy = (int)(sinf(angle) * 30.0f);
            float opacity = (float)(i + 1) / 8.0f;
            draw_circle(cx + dx, cy + dy, 4, COLOR_TAUPE, opacity);
        }
        
        draw_string_bold(cx - 50, cy + 60, "INITIALIZING FATEOS", COLOR_MUTED);
        
        render_cursor();
        blit_screen();
        usleep(16666); // ~60fps
    }

    // Main Login Loop
    while (1) {
        poll_input();
        
        // Hover detection
        int is_hovering = 0;
        
        // 1. Username field box
        int user_x1 = card_x1 + 40, user_x2 = card_x2 - 40;
        int user_y1 = card_y1 + 100, user_y2 = card_y1 + 135;
        if (mx >= user_x1 && mx <= user_x2 && my >= user_y1 && my <= user_y2) {
            is_hovering = 1;
        }
        
        // 2. Password field box
        int pass_x1 = card_x1 + 40, pass_x2 = card_x2 - 40;
        int pass_y1 = card_y1 + 170, pass_y2 = card_y1 + 205;
        if (mx >= pass_x1 && mx <= pass_x2 && my >= pass_y1 && my <= pass_y2) {
            is_hovering = 1;
        }

        // 3. Login Button
        int btn_x1 = card_x1 + 40, btn_x2 = card_x2 - 40;
        int btn_y1 = card_y1 + 240, btn_y2 = card_y1 + 280;
        if (mx >= btn_x1 && mx <= btn_x2 && my >= btn_y1 && my <= btn_y2) {
            is_hovering = 1;
        }

        // 4. Shutdown / Reboot Buttons
        int shut_x1 = 50, shut_x2 = 180, shut_y1 = height - 60, shut_y2 = height - 20;
        int reb_x1 = 200, reb_x2 = 330, reb_y1 = height - 60, reb_y2 = height - 20;
        if ((mx >= shut_x1 && mx <= shut_x2 && my >= shut_y1 && my <= shut_y2) ||
            (mx >= reb_x1 && mx <= reb_x2 && my >= reb_y1 && my <= reb_y2)) {
            is_hovering = 1;
        }

        // Cursor state
        if (is_hovering) {
            target_ring_size = 56.0f;
            ring_color = COLOR_CORAL;
        } else {
            target_ring_size = 40.0f;
            ring_color = COLOR_TAUPE;
        }

        // Handle clicks
        if (left_button_pressed) {
            // Click username box
            if (mx >= user_x1 && mx <= user_x2 && my >= user_y1 && my <= user_y2) {
                active_field = 0;
            }
            // Click password box
            else if (mx >= pass_x1 && mx <= pass_x2 && my >= pass_y1 && my <= pass_y2) {
                active_field = 1;
            }
            // Click Login Button
            else if (mx >= btn_x1 && mx <= btn_x2 && my >= btn_y1 && my <= btn_y2) {
                goto do_login;
            }
            // Click Shutdown
            else if (mx >= shut_x1 && mx <= shut_x2 && my >= shut_y1 && my <= shut_y2) {
                printf("[-] Shutting down cleanly...\n");
                system("poweroff -f");
                return 0;
            }
            // Click Reboot
            else if (mx >= reb_x1 && mx <= reb_x2 && my >= reb_y1 && my <= reb_y2) {
                printf("[-] Rebooting...\n");
                system("reboot -f");
                return 0;
            }
            
            // Release mouse click immediately to prevent multi-triggering
            left_button_pressed = 0;
        }

        // Handle keyboard characters
        int key = get_keyboard_char();
        if (key != -1) {
            if (key == KEY_ESC) {
                // ESC to exit GUI safely in case of debugging
                return 0;
            } else if (key == '\t') {
                active_field = 1 - active_field; // Switch fields
            } else if (key == KEY_ENTER) {
                if (active_field == 0 && strlen(username) > 0) {
                    active_field = 1; // move to password
                } else {
                    goto do_login;
                }
            } else if (key == KEY_BACKSPACE) {
                if (active_field == 0) {
                    int len = strlen(username);
                    if (len > 0) username[len - 1] = '\0';
                } else {
                    int len = strlen(password);
                    if (len > 0) password[len - 1] = '\0';
                }
            } else if (key >= 32 && key <= 126) {
                if (active_field == 0) {
                    int len = strlen(username);
                    if (len < 31) {
                        username[len] = (char)key;
                        username[len + 1] = '\0';
                    }
                } else {
                    int len = strlen(password);
                    if (len < 31) {
                        password[len] = (char)key;
                        password[len + 1] = '\0';
                    }
                }
            }
        }

        // Render Frame
        clear_screen();
        draw_desktop_background();

        // Logo Top-Left
        draw_string_bold(50, 40, "D5BU.", COLOR_TEXT);
        draw_string(110, 44, "|  SECURE PORTAL", COLOR_MUTED);

        // Core Login Card
        uint32_t card_border_color = (error_timer > 0) ? COLOR_CORAL : COLOR_BORDER;
        draw_rect(card_x1, card_y1, card_x2, card_y2, COLOR_SURFACE, 0.90f); // Translucent card
        draw_border(card_x1, card_y1, card_x2, card_y2, card_border_color, 0.25f);

        // Card title
        draw_string_bold(card_x1 + 40, card_y1 + 35, "FATE AUTHENTICATION", COLOR_TEXT);
        draw_rect(card_x1 + 40, card_y1 + 55, card_x2 - 40, card_y1 + 56, COLOR_BORDER, 0.15f);

        // Username field rendering
        draw_string(user_x1, user_y1 - 18, "USER IDENTIFICATION", COLOR_MUTED);
        draw_rect(user_x1, user_y1, user_x2, user_y2, COLOR_BG, 1.0f);
        uint32_t user_border = (active_field == 0) ? COLOR_TAUPE : COLOR_BORDER;
        draw_border(user_x1, user_y1, user_x2, user_y2, user_border, (active_field == 0) ? 0.8f : 0.2f);
        draw_string(user_x1 + 12, user_y1 + 10, username, COLOR_TEXT);
        if (active_field == 0 && (active_field == 0)) {
            // Blinking cursor simulator
            int cursor_x = user_x1 + 12 + strlen(username) * 8;
            draw_rect(cursor_x, user_y1 + 8, cursor_x + 1, user_y2 - 8, COLOR_TAUPE, 1.0f);
        }

        // Password field rendering
        draw_string(pass_x1, pass_y1 - 18, "ACCESS KEY", COLOR_MUTED);
        draw_rect(pass_x1, pass_y1, pass_x2, pass_y2, COLOR_BG, 1.0f);
        uint32_t pass_border = (active_field == 1) ? COLOR_TAUPE : COLOR_BORDER;
        draw_border(pass_x1, pass_y1, pass_x2, pass_y2, pass_border, (active_field == 1) ? 0.8f : 0.2f);
        
        // Draw asterisks instead of plain text
        char stars[32] = "";
        int pass_len = strlen(password);
        for (int i = 0; i < pass_len; i++) stars[i] = '*';
        stars[pass_len] = '\0';
        draw_string(pass_x1 + 12, pass_y1 + 10, stars, COLOR_TEXT);
        
        if (active_field == 1) {
            int cursor_x = pass_x1 + 12 + pass_len * 8;
            draw_rect(cursor_x, pass_y1 + 8, cursor_x + 1, pass_y2 - 8, COLOR_TAUPE, 1.0f);
        }

        // Login Button
        uint32_t btn_bg = COLOR_SURFACE;
        uint32_t btn_txt_color = COLOR_TAUPE;
        float btn_opacity = 0.5f;
        // Check if mouse is over button or active field highlights it
        if (mx >= btn_x1 && mx <= btn_x2 && my >= btn_y1 && my <= btn_y2) {
            btn_bg = COLOR_TAUPE;
            btn_txt_color = 0x000000;
            btn_opacity = 1.0f;
        }
        draw_rect(btn_x1, btn_y1, btn_x2, btn_y2, btn_bg, btn_opacity);
        draw_border(btn_x1, btn_y1, btn_x2, btn_y2, COLOR_TAUPE, 0.6f);
        draw_string_bold(btn_x1 + 130, btn_y1 + 13, "ENTER PORTAL", btn_txt_color);

        // Error message handling
        if (error_timer > 0) {
            draw_string(card_x1 + 40, card_y2 - 25, error_msg, COLOR_CORAL);
            error_timer--;
        } else {
            draw_string(card_x1 + 40, card_y2 - 25, "PRESS TAB TO SWITCH FIELDS", COLOR_MUTED);
        }

        // Shutdown and Reboot Buttons
        draw_rect(shut_x1, shut_y1, shut_x2, shut_y2, COLOR_SURFACE, 1.0f);
        draw_border(shut_x1, shut_y1, shut_x2, shut_y2, COLOR_CORAL, 0.3f);
        draw_string_bold(shut_x1 + 35, shut_y1 + 13, "POWER OFF", COLOR_CORAL);

        draw_rect(reb_x1, reb_y1, reb_x2, reb_y2, COLOR_SURFACE, 1.0f);
        draw_border(reb_x1, reb_y1, reb_x2, reb_y2, COLOR_TAUPE, 0.3f);
        draw_string_bold(reb_x1 + 45, reb_y1 + 13, "REBOOT", COLOR_TAUPE);

        // Render signature responsive mouse cursor
        render_cursor();

        blit_screen();
        usleep(16666);
        continue;

    do_login:
        // Validate credentials
        // Accepted: username 'fate' or 'root', password 'fate' or empty/root
        if ((strcmp(username, "fate") == 0 && strcmp(password, "fate") == 0) ||
            (strcmp(username, "root") == 0 && (strcmp(password, "root") == 0 || strcmp(password, "") == 0))) {
            
            // Success transition: Flash to white and fade into desktop
            printf("[+] Login authorized! Starting user session...\n");
            for (int f = 0; f < 15; f++) {
                clear_screen();
                draw_desktop_background();
                // Draw a beautiful white overlay fading in
                draw_rect(0, 0, width, height, 0xffffff, (float)f / 15.0f);
                blit_screen();
                usleep(16666);
            }
            return 1;
        } else {
            // Failure sequence
            strcpy(error_msg, "ACCESS CREDENTIALS INVALID");
            error_timer = 120; // 2 seconds
            memset(password, 0, sizeof(password)); // Clear key
        }
    }
}
