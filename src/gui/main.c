#include "core.h"
#include "input.h"
#include "fatedm.h"
#include "fatede.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("[*] Launching FateOS Graphical System...\n");

    // Initialize low-level VESA graphics double buffer
    if (init_graphics() != 0) {
        fprintf(stderr, "[!] Fatal Error: Graphics initialization failed.\n");
        return 1;
    }
    printf("[*] Framebuffer initialized: %dx%d (stride: %d)\n", width, height, line_len);


    // Initialize keyboard and mouse input devices
    if (init_input() != 0) {
        fprintf(stderr, "[!] Fatal Error: Input initialization failed.\n");
        close_graphics();
        return 1;
    }

    // Phase 1: Boot into FateDM Display Manager (Login Screen)
    int session_authorized = run_login_screen();

    if (session_authorized) {
        // Phase 2: Start FateDE Desktop Environment
        init_desktop();
        run_desktop_loop();
    }

    // Clean up resources if system exits (shutdown/reboot or ESC debug exit)
    close_input();
    close_graphics();
    printf("[*] Graphical System terminated cleanly.\n");

    return 0;
}
