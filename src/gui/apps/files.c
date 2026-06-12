#include "../fatede.h"
#include "../core.h"
#include "apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_ENTRIES 20

typedef struct {
    char name[128];
    int is_dir;
    unsigned long size;
} FileEntry;

typedef struct {
    char current_path[256];
    FileEntry entries[MAX_ENTRIES];
    int entry_count;
    int selected_entry;
    char file_detail_msg[128];
} FileState;

static void load_directory(FileState *state) {
    state->entry_count = 0;
    state->selected_entry = -1;
    state->file_detail_msg[0] = '\0';
    
    DIR *dir = opendir(state->current_path);
    if (!dir) {
        strcpy(state->file_detail_msg, "ERR: UNABLE TO OPEN PATH");
        return;
    }
    
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        // Skip "."
        if (strcmp(dp->d_name, ".") == 0) continue;
        
        FileEntry *ent = &state->entries[state->entry_count];
        strncpy(ent->name, dp->d_name, sizeof(ent->name) - 1);
        
        // Query details
        char full_path[512];
        if (strcmp(state->current_path, "/") == 0) {
            sprintf(full_path, "/%s", dp->d_name);
        } else {
            sprintf(full_path, "%s/%s", state->current_path, dp->d_name);
        }
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            ent->is_dir = S_ISDIR(st.st_mode);
            ent->size = (unsigned long)st.st_size;
        } else {
            ent->is_dir = (dp->d_type == DT_DIR);
            ent->size = 0;
        }
        
        state->entry_count++;
        if (state->entry_count >= MAX_ENTRIES) break;
    }
    closedir(dir);
}


static void draw_files(int x, int y, int w, int h, void *data) {
    FileState *state = (FileState *)data;
    
    // Background canvas
    draw_rect(x + 10, y + 40, x + w - 10, y + h - 10, COLOR_BG, 1.0f);
    draw_border(x + 10, y + 40, x + w - 10, y + h - 10, COLOR_BORDER, 0.2f);
    
    // Path title bar
    draw_string(x + 18, y + 15, "PATH:", COLOR_MUTED);
    draw_string_bold(x + 60, y + 14, state->current_path, COLOR_TEXT);
    
    // Back button (local coord: local_x 300 to 365, local_y 10 to 32)
    int btn_x1 = x + w - 80;
    int btn_y1 = y + 10;
    int btn_x2 = x + w - 15;
    int btn_y2 = y + 32;
    draw_rect(btn_x1, btn_y1, btn_x2, btn_y2, COLOR_SURFACE, 1.0f);
    draw_border(btn_x1, btn_y1, btn_x2, btn_y2, COLOR_TAUPE, 0.5f);
    draw_string(btn_x1 + 24, btn_y1 + 5, "UP", COLOR_TAUPE);
    
    // Render file entries
    int start_x = x + 20;
    int start_y = y + 50;
    
    for (int i = 0; i < state->entry_count; i++) {
        FileEntry *ent = &state->entries[i];
        int item_y = start_y + i * 16;
        if (item_y >= y + h - 35) break; // Clamp output limits
        
        // Highlight active clicked entry
        if (i == state->selected_entry) {
            draw_rect(start_x - 5, item_y - 2, x + w - 15, item_y + 14, COLOR_SURFACE, 0.5f);
        }
        
        uint32_t color = ent->is_dir ? COLOR_TAUPE : COLOR_TEXT;
        char prefix[16];
        sprintf(prefix, ent->is_dir ? "[DIR] " : "      ");
        draw_string(start_x, item_y, prefix, COLOR_MUTED);
        draw_string(start_x + 48, item_y, ent->name, color);
    }
    
    // Bottom detail bar
    if (strlen(state->file_detail_msg) > 0) {
        draw_string(x + 20, y + h - 25, state->file_detail_msg, COLOR_CORAL);
    } else if (state->selected_entry != -1) {
        FileEntry *ent = &state->entries[state->selected_entry];
        char detail[128];
        if (ent->is_dir) {
            sprintf(detail, "DIRECTORY: %s", ent->name);
        } else {
            sprintf(detail, "FILE: %s (%lu bytes)", ent->name, ent->size);
        }
        draw_string(x + 20, y + h - 25, detail, COLOR_MUTED);
    } else {
        draw_string(x + 20, y + h - 25, "DOUBLE CLICK DIRECTORY TO NAVIGATE", COLOR_MUTED);
    }
}

static void handle_click_files(int local_x, int local_y, int button, void *data) {
    FileState *state = (FileState *)data;
    
    // Check "UP" button click (local_x >= 300, local_y >= 10 && local_y <= 32)
    if (local_y >= 10 && local_y <= 32 && local_x >= 300) {
        if (strcmp(state->current_path, "/") != 0) {
            // Find parent path
            char *last_slash = strrchr(state->current_path, '/');
            if (last_slash == state->current_path) {
                strcpy(state->current_path, "/");
            } else if (last_slash != NULL) {
                *last_slash = '\0';
            }
            load_directory(state);
        }
        return;
    }
    
    // Check list entries click
    int list_y1 = 50;
    int click_idx = (local_y - list_y1) / 16;
    if (click_idx >= 0 && click_idx < state->entry_count) {
        if (state->selected_entry == click_idx) {
            // Double click triggered: Traverse directory
            FileEntry *ent = &state->entries[click_idx];
            if (ent->is_dir) {
                char new_path[512];
                if (strcmp(state->current_path, "/") == 0) {
                    sprintf(new_path, "/%s", ent->name);
                } else {
                    sprintf(new_path, "%s/%s", state->current_path, ent->name);
                }
                strcpy(state->current_path, new_path);
                load_directory(state);
            } else {
                // If it's a file, set detail message
                sprintf(state->file_detail_msg, "FILE: %s (%lu B)", ent->name, ent->size);
            }
        } else {
            state->selected_entry = click_idx;
            state->file_detail_msg[0] = '\0';
        }
    }
}

static void on_close_files(void *data) {
    if (data) free(data);
}

void spawn_file_manager() {
    FileState *state = (FileState *)malloc(sizeof(FileState));
    if (!state) return;
    memset(state, 0, sizeof(FileState));
    
    strcpy(state->current_path, "/");
    load_directory(state);
    
    int win_w = 380;
    int win_h = 300;
    int win_x = (width - win_w) / 2 - 80;
    int win_y = (height - win_h) / 2 + 10;
    
    Window *win = create_window("Loom Files", win_x, win_y, win_w, win_h, 
                                draw_files, handle_click_files, NULL, state);
    if (win) {
        win->on_close = on_close_files;
    }
}
