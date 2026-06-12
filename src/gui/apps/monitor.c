#include "../fatede.h"
#include "../core.h"
#include "apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CPU_HISTORY_COUNT 40

typedef struct {
    float cpu_history[CPU_HISTORY_COUNT];
    int history_index;
    
    // CPU calculation state
    unsigned long long prev_total;
    unsigned long long prev_idle;
    
    // RAM state
    unsigned long long total_mem;
    unsigned long long free_mem;
    
    int update_counter;
} MonitorState;

static void read_cpu_usage(MonitorState *state) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    
    char buf[128];
    if (fgets(buf, sizeof(buf), fp)) {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        if (sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", 
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
            unsigned long long idle_all = idle + iowait;
            unsigned long long total = user + nice + system + idle_all + irq + softirq + steal;
            
            unsigned long long diff_total = total - state->prev_total;
            unsigned long long diff_idle = idle_all - state->prev_idle;
            
            float cpu_percentage = 0.0f;
            if (diff_total > 0) {
                cpu_percentage = (float)(diff_total - diff_idle) / (float)diff_total * 100.0f;
            }
            
            state->cpu_history[state->history_index] = cpu_percentage;
            state->history_index = (state->history_index + 1) % CPU_HISTORY_COUNT;
            
            state->prev_total = total;
            state->prev_idle = idle_all;
        }
    }
    fclose(fp);
}

static void read_ram_usage(MonitorState *state) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return;
    
    char line[128];
    unsigned long long mem_total = 0, mem_avail = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %llu kB", &mem_total);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %llu kB", &mem_avail);
        }
    }
    fclose(fp);
    
    if (mem_total > 0) {
        state->total_mem = mem_total;
        state->free_mem = mem_avail;
    }
}

static void draw_monitor(int x, int y, int w, int h, void *data) {
    MonitorState *state = (MonitorState *)data;
    
    // Read stats every ~30 frames (approx 0.5s)
    state->update_counter++;
    if (state->update_counter >= 30) {
        read_cpu_usage(state);
        read_ram_usage(state);
        state->update_counter = 0;
    }
    
    // Draw background grid
    draw_rect(x + 10, y + 10, x + w - 10, y + h - 10, COLOR_BG, 1.0f);
    
    // CPU Section
    draw_string_bold(x + 20, y + 20, "CPU UTILIZATION", COLOR_TAUPE);
    
    // Render historical CPU graph
    int graph_x = x + 20;
    int graph_y = y + 40;
    int graph_w = w - 40;
    int graph_h = 80;
    
    // Draw graph border
    draw_border(graph_x, graph_y, graph_x + graph_w, graph_y + graph_h, COLOR_BORDER, 0.2f);
    
    // Draw horizontal grid lines (25%, 50%, 75%)
    for (int pct = 25; pct <= 75; pct += 25) {
        int line_y = graph_y + graph_h - (pct * graph_h / 100);
        draw_rect(graph_x + 1, line_y, graph_x + graph_w - 1, line_y, COLOR_BORDER, 0.08f);
    }
    
    // Plot history data points
    int step_w = graph_w / (CPU_HISTORY_COUNT - 1);
    int prev_px = -1, prev_py = -1;
    for (int i = 0; i < CPU_HISTORY_COUNT; i++) {
        int idx = (state->history_index + i) % CPU_HISTORY_COUNT;
        float val = state->cpu_history[idx];
        if (val < 0.0f) val = 0.0f;
        if (val > 100.0f) val = 100.0f;
        
        int px = graph_x + i * step_w;
        int py = graph_y + graph_h - (int)(val * (float)graph_h / 100.0f);
        
        if (prev_px != -1) {
            // Draw simple lines between points (coarse horizontal line segment representation)
            int min_x = (prev_px < px) ? prev_px : px;
            int max_x = (prev_px > px) ? prev_px : px;
            for (int lx = min_x; lx <= max_x; lx++) {
                // interpolate
                float t = (float)(lx - prev_px) / (float)(px - prev_px);
                int ly = prev_py + (int)((py - prev_py) * t);
                put_pixel(lx, ly, COLOR_CORAL);
            }
        }
        prev_px = px;
        prev_py = py;
    }
    
    // Draw current CPU value
    int current_idx = (state->history_index + CPU_HISTORY_COUNT - 1) % CPU_HISTORY_COUNT;
    char cpu_text[32];
    sprintf(cpu_text, "CURRENT: %.1f%%", state->cpu_history[current_idx]);
    draw_string(x + w - 140, y + 20, cpu_text, COLOR_CORAL);
    
    // RAM Section
    int ram_y = y + 140;
    draw_string_bold(x + 20, ram_y, "MEMORY ALLOCATION", COLOR_TAUPE);
    
    unsigned long long total_mb = state->total_mem / 1024;
    unsigned long long used_mb = (state->total_mem - state->free_mem) / 1024;
    float ram_pct = 0.0f;
    if (state->total_mem > 0) {
        ram_pct = (float)(state->total_mem - state->free_mem) / (float)state->total_mem * 100.0f;
    }
    
    char ram_text[64];
    sprintf(ram_text, "%llu MB / %llu MB (%.1f%%)", used_mb, total_mb, ram_pct);
    draw_string(x + 20, ram_y + 20, ram_text, COLOR_TEXT);
    
    // RAM Progress Bar
    int bar_x = x + 20;
    int bar_y = ram_y + 40;
    int bar_w = w - 40;
    int bar_h = 16;
    draw_rect(bar_x, bar_y, bar_x + bar_w, bar_y + bar_h, COLOR_BG, 1.0f);
    draw_border(bar_x, bar_y, bar_x + bar_w, bar_y + bar_h, COLOR_BORDER, 0.3f);
    
    int fill_w = (int)((float)bar_w * ram_pct / 100.0f);
    if (fill_w > bar_w) fill_w = bar_w;
    if (fill_w > 0) {
        draw_rect(bar_x + 2, bar_y + 2, bar_x + fill_w - 2, bar_y + bar_h - 2, COLOR_TAUPE, 0.8f);
    }
}

static void on_close_monitor(void *data) {
    if (data) free(data);
}

void spawn_system_monitor() {
    MonitorState *state = (MonitorState *)malloc(sizeof(MonitorState));
    if (!state) return;
    memset(state, 0, sizeof(MonitorState));
    
    // Initialize history with 0s
    for (int i = 0; i < CPU_HISTORY_COUNT; i++) state->cpu_history[i] = 0.0f;
    state->history_index = 0;
    state->update_counter = 30; // Force immediate update on launch
    
    // Center it on screen
    int win_w = 400;
    int win_h = 240;
    int win_x = (width - win_w) / 2 - 30;
    int win_y = (height - win_h) / 2 - 40;
    
    Window *win = create_window("Loom Monitor", win_x, win_y, win_w, win_h, 
                                draw_monitor, NULL, NULL, state);
    if (win) {
        win->on_close = on_close_monitor;
    }
}
