#include "input.h"
#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <string.h>

int mx = 512;
int my = 384;
float rx = 512.0f;
float ry = 384.0f;
float ring_size = 40.0f;
float target_ring_size = 40.0f;
uint32_t ring_color = COLOR_TAUPE;
int left_button_pressed = 0;

static int mouse_fd = -1;
static int tty_fd = -1;
static struct termios old_tty;
static int keyboard_inited = 0;

// Internal queue to hold keystrokes
#define KEY_QUEUE_SIZE 64
static int key_queue[KEY_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;

static void enqueue_key(int key) {
    int next = (queue_tail + 1) % KEY_QUEUE_SIZE;
    if (next != queue_head) {
        key_queue[queue_tail] = key;
        queue_tail = next;
    }
}

int init_input() {
    printf("[-] Initializing Input devices...\n");

    // Initialize mouse
    mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
    if (mouse_fd < 0) {
        printf("[!] Warning: Cannot open /dev/input/mice (relative mouse might not work).\n");
    }

    // Initialize keyboard on tty1
    tty_fd = open("/dev/tty1", O_RDWR | O_NONBLOCK);
    if (tty_fd < 0) {
        // Fallback to stdin
        tty_fd = dup(STDIN_FILENO);
        fcntl(tty_fd, F_SETFL, O_NONBLOCK);
    }

    if (tty_fd >= 0) {
        if (tcgetattr(tty_fd, &old_tty) >= 0) {
            struct termios raw = old_tty;
            // Set raw mode: disable echo, canonical mode, signals, and extended functions
            raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
            raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
            raw.c_cflag |= (CS8);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(tty_fd, TCSAFLUSH, &raw) >= 0) {
                keyboard_inited = 1;
                printf("[+] Keyboard successfully initialized in raw mode.\n");
                
                // Set the console to graphics mode to disable text cursor/echo overwrites
                if (ioctl(tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
                    perror("[!] Warning: Cannot set KD_GRAPHICS on tty");
                }
            }
        }
    }

    return 0;
}

void close_input() {
    if (keyboard_inited && tty_fd >= 0) {
        // Restore console to text mode
        if (ioctl(tty_fd, KDSETMODE, KD_TEXT) < 0) {
            perror("[!] Warning: Cannot restore KD_TEXT on tty");
        }
        tcsetattr(tty_fd, TCSAFLUSH, &old_tty);
        keyboard_inited = 0;
    }
    if (tty_fd >= 0) {
        close(tty_fd);
        tty_fd = -1;
    }
    if (mouse_fd >= 0) {
        close(mouse_fd);
        mouse_fd = -1;
    }
}

void poll_input() {
    // 1. Poll Mouse
    if (mouse_fd >= 0) {
        signed char mouse_data[3];
        int bytes = read(mouse_fd, mouse_data, sizeof(mouse_data));
        if (bytes == 3) {
            int dx = mouse_data[1];
            int dy = mouse_data[2];
            mx += dx;
            my -= dy; // relative vertical is inverted

            // Clamp coordinates to screen bounds
            if (mx < 0) mx = 0;
            if (mx >= width) mx = width - 1;
            if (my < 0) my = 0;
            if (my >= height) my = height - 1;

            left_button_pressed = mouse_data[0] & 0x1;
        }
    }

    // 2. Poll Keyboard
    if (tty_fd >= 0) {
        unsigned char buf[8];
        int bytes = read(tty_fd, buf, sizeof(buf));
        if (bytes > 0) {
            for (int i = 0; i < bytes; i++) {
                if (buf[i] == 27) { // Escape sequence check
                    if (i + 2 < bytes && buf[i+1] == '[') {
                        if (buf[i+2] == 'A') enqueue_key(KEY_UP);
                        else if (buf[i+2] == 'B') enqueue_key(KEY_DOWN);
                        else if (buf[i+2] == 'C') enqueue_key(KEY_RIGHT);
                        else if (buf[i+2] == 'D') enqueue_key(KEY_LEFT);
                        i += 2;
                    } else {
                        enqueue_key(KEY_ESC);
                    }
                } else if (buf[i] == 127 || buf[i] == 8) {
                    enqueue_key(KEY_BACKSPACE);
                } else if (buf[i] == 13 || buf[i] == 10) {
                    enqueue_key(KEY_ENTER);
                } else if (buf[i] >= 32 && buf[i] <= 126) {
                    enqueue_key(buf[i]);
                }
            }
        }
    }

    // 3. Interpolate cursor position for lag effect (signature portfolio micro-animation)
    rx += (mx - rx) * 0.15f;
    ry += (my - ry) * 0.15f;
    ring_size += (target_ring_size - ring_size) * 0.15f;
}

int get_keyboard_char() {
    if (queue_head == queue_tail) {
        return -1; // Empty queue
    }
    int key = key_queue[queue_head];
    queue_head = (queue_head + 1) % KEY_QUEUE_SIZE;
    return key;
}

void render_cursor() {
    // 1. Draw outer trailing ring with lag
    draw_circle_outline((int)rx, (int)ry, (int)(ring_size / 2.0f), ring_color, 1.0f);

    // 2. Draw solid inner dot
    for (int cy = -4; cy <= 4; cy++) {
        for (int cx = -4; cx <= 4; cx++) {
            int px = mx + cx;
            int py = my + cy;
            if (cx * cx + cy * cy <= 16) {
                put_pixel(px, py, COLOR_TAUPE);
            }
        }
    }
}
