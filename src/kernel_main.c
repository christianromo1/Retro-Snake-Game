#include <stdint.h>

// NEVER DELETE NEXT TWO LINES OF CODE FOR BOOT TO GRUB
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

// Minimal multiboot2 header (keep it simple/standard)
const unsigned int multiboot_header[]
__attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC)
};
// CAN MAKE EDITS PAST THIS

// (Not required for this assignment, but harmless to keep if your template uses it)
uint8_t inb(uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "dN"(_port));
    return rv;
}

// Global variables to track cursor position
int x = 0;
int y = 0;

// Constants for screen dimensions
#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25
#define VIDEO_MEMORY  0xB8000

// Color attribute: light gray text on black background
#define COLOR 0x07

static void scroll(void) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;

    // Move rows 1..24 up to 0..23
    for (int row = 1; row < SCREEN_HEIGHT; row++) {
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            int src = (row * SCREEN_WIDTH + col) * 2;
            int dst = ((row - 1) * SCREEN_WIDTH + col) * 2;
            vram[dst]     = vram[src];
            vram[dst + 1] = vram[src + 1];
        }
    }

    // Clear last row
    for (int col = 0; col < SCREEN_WIDTH; col++) {
        int off = ((SCREEN_HEIGHT - 1) * SCREEN_WIDTH + col) * 2;
        vram[off]     = ' ';
        vram[off + 1] = COLOR;
    }
}

// Writes a single character at (x,y) and advances cursor; scrolls if needed.
void putc(int data) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;
    unsigned char c = (unsigned char)data;

    // Newline handling
    if (c == '\n') {
        x = 0;
        y++;
    } else {
        int off = (y * SCREEN_WIDTH + x) * 2;
        vram[off]     = c;
        vram[off + 1] = COLOR;

        x++;
        if (x >= SCREEN_WIDTH) {
            x = 0;
            y++;
        }
    }

    // Scroll if we went past the last row
    if (y >= SCREEN_HEIGHT) {
        scroll();
        y = SCREEN_HEIGHT - 1;
        // keep x as-is (don’t force to 0)
        if (x >= SCREEN_WIDTH) x = 0; // safety
    }
}

// Writes a NULL-terminated string by calling putc() for each character.
void puts(const char *str) {
    while (*str) {
        putc(*str++);
    }
}

static void clear_screen(void) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;

    for (int row = 0; row < SCREEN_HEIGHT; row++) {
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            int off = (row * SCREEN_WIDTH + col) * 2;
            vram[off]     = ' ';
            vram[off + 1] = COLOR;
        }
    }

    x = 0;
    y = 0;
}

void main(void) {
    clear_screen();

    // Simple test output that clearly demonstrates:
    // - sequential printing
    // - newline handling
    // - scrolling past row 24
    puts("Terminal driver test\n");
    puts("--------------------\n");

    for (int i = 0; i < 30; i++) {
        puts("Scrolling test line\n");
    }

    while (1) { }
}
