#include <stdint.h>

// NEVER DELETE NEXT TWO LINES OF CODE FOR BOOT TO GRUB
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

const unsigned int multiboot_header[]
__attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC)
};

// CAN MAKE EDITS PAST THIS

uint8_t inb(uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "dN"(_port));
    return rv;
}

// Keyboard scancode to ASCII mapping table
unsigned char keyboard_map[128] =
{
   0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
 '9', '0', '-', '=', '\b',     /* Backspace */
 '\t',                 /* Tab */
 'q', 'w', 'e', 'r',   /* 19 */
 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
   0,                  /* 29   - Control */
 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 39 */
'\'', '`',   0,                /* Left shift */
'\\', 'z', 'x', 'c', 'v', 'b', 'n',                    /* 49 */
 'm', ',', '.', '/',   0,                              /* Right shift */
 '*',
   0,  /* Alt */
 ' ',  /* Space bar */
   0,  /* Caps lock */
   0,  /* 59 - F1 key ... > */
   0,   0,   0,   0,   0,   0,   0,   0,  
   0,  /* < ... F10 */
   0,  /* 69 - Num lock*/
   0,  /* Scroll Lock */
   0,  /* Home key */
   0,  /* Up Arrow */
   0,  /* Page Up */
 '-',
   0,  /* Left Arrow */
   0,  
   0,  /* Right Arrow */
 '+',
   0,  /* 79 - End key*/
   0,  /* Down Arrow */
   0,  /* Page Down */
   0,  /* Insert Key */
   0,  /* Delete Key */
   0,   0,   0,  
   0,  /* F11 Key */
   0,  /* F12 Key */
   0,  /* All other keys are undefined */
};

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
    } else if (c == '\b') {
        // Backspace handling
        if (x > 0) {
            x--;
            int off = (y * SCREEN_WIDTH + x) * 2;
            vram[off] = ' ';
            vram[off + 1] = COLOR;
        }
    } else if (c == '\t') {
        // Tab handling - move to next multiple of 4
        x = (x + 4) & ~3;
        if (x >= SCREEN_WIDTH) {
            x = 0;
            y++;
        }
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
        if (x >= SCREEN_WIDTH) x = 0;
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
    
    puts("Keyboard driver ready. Start typing!\n");
    
    while (1) {
        uint8_t status = inb(0x64);
        
        if (status & 1) {
            uint8_t scancode = inb(0x60);
            
            // THE ONE LINE OF CODE - translate scancode to ASCII and print
            if (scancode < 128) putc(keyboard_map[scancode]);
        }
    }
}
