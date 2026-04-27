#include <stdint.h>
#include "page.h"
#include "paging.h"
#include "fat.h"
#include "ide.h"
#include "graphics.h"
#include "snake.h"          // <<< NEW

// FAT driver function prototypes
int fatInit(void);
struct file *fatOpen(const char *filename);
int fatRead(struct file *f, char *buf, uint32_t nbytes);

// NEVER DELETE NEXT FEW LINES OF CODE FOR BOOT TO GRUB
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
const unsigned int multiboot_header[]
__attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC,
    0,
    48,
    -(MULTIBOOT2_HEADER_MAGIC + 0 + 48),
    5, 20, 640, 480, 32, 0,
    0, 8
};
// CAN MAKE EDITS PAST THIS

extern uint8_t _end_kernel;

// ============ INTERRUPT STRUCTURES ============

struct idt_gate {
    uint16_t offset_1;
    uint16_t cs_selector;
    uint8_t  zero;
    uint8_t  attributes;
    uint16_t offset_2;
};

struct idt_descriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_gate interrupt_descripter_table[256];

// ============ I/O FUNCTIONS ============

uint8_t inb(uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__("inb %1, %0" : "=a"(rv) : "dN"(_port));
    return rv;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

// ============ KEYBOARD MAP ============

unsigned char keyboard_map[128] =
{
   0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
 '9', '0', '-', '=', '\b',
 '\t',
 'q', 'w', 'e', 'r',
 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
   0,
 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
'\'', '`',   0,
'\\', 'z', 'x', 'c', 'v', 'b', 'n',
 'm', ',', '.', '/',   0,
 '*',   0, ' ',   0,
   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0, '-',
   0,   0,   0, '+',
   0,   0,   0,   0,   0,
   0,   0,   0,
};

// ============ SCREEN DRIVER (kept for debug — not visible in framebuffer mode) ============

int x = 0;
int y = 0;

#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25
#define VIDEO_MEMORY  0xB8000
#define COLOR 0x07

static void scroll(void) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;
    for (int row = 1; row < SCREEN_HEIGHT; row++)
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            int src = (row * SCREEN_WIDTH + col) * 2;
            int dst = ((row-1) * SCREEN_WIDTH + col) * 2;
            vram[dst]   = vram[src];
            vram[dst+1] = vram[src+1];
        }
    for (int col = 0; col < SCREEN_WIDTH; col++) {
        int off = ((SCREEN_HEIGHT-1) * SCREEN_WIDTH + col) * 2;
        vram[off] = ' '; vram[off+1] = COLOR;
    }
}

void putc(int data) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;
    unsigned char c = (unsigned char)data;
    if (c == '\n') { x = 0; y++; }
    else if (c == '\b') { if (x > 0) { x--; int off=(y*SCREEN_WIDTH+x)*2; vram[off]=' '; vram[off+1]=COLOR; } }
    else if (c == '\t') { x=(x+4)&~3; if(x>=SCREEN_WIDTH){x=0;y++;} }
    else {
        int off=(y*SCREEN_WIDTH+x)*2;
        vram[off]=c; vram[off+1]=COLOR; x++;
        if(x>=SCREEN_WIDTH){x=0;y++;}
    }
    if(y>=SCREEN_HEIGHT){scroll();y=SCREEN_HEIGHT-1;if(x>=SCREEN_WIDTH)x=0;}
}

void puts(const char *str) { while(*str) putc(*str++); }

static void vga_clear(void) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;
    for (int row = 0; row < SCREEN_HEIGHT; row++)
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            int off=(row*SCREEN_WIDTH+col)*2;
            vram[off]=' '; vram[off+1]=COLOR;
        }
    x=0; y=0;
}

// ============ INTERRUPT HANDLERS ============

__attribute__((interrupt)) void dummy_handler(void *p) {}

// <<< CHANGED: handles extended arrow key scancodes for game direction
__attribute__((interrupt)) void keyboard_handler(void *p) {
    static int extended = 0;
    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) {
        extended = 1;
    } else if (extended) {
        extended = 0;
        if (game_is_over()) {
            // Any arrow key restarts
            if (scancode==0x48||scancode==0x50||scancode==0x4B||scancode==0x4D)
                game_init(0xCAFEBABE);
        } else {
            switch (scancode) {
                case 0x48: game_set_direction(DIR_UP);    break;
                case 0x50: game_set_direction(DIR_DOWN);  break;
                case 0x4B: game_set_direction(DIR_LEFT);  break;
                case 0x4D: game_set_direction(DIR_RIGHT); break;
            }
        }
    }
    outb(0x20, 0x20);
}

// <<< CHANGED: drives game tick every 6 PIT interrupts (~3 moves/sec)
__attribute__((interrupt)) void pit_handler(void *p) {
    static unsigned int ticks = 0;
    static unsigned int total = 0;
    ticks++;
    total++;
    if (ticks >= 4) {
        ticks = 0;
        game_tick();
    }
    outb(0x20, 0x20);
}

// ============ INTERRUPT SETUP ============

void set_idt_gate(void *handler_address, unsigned int gate_number) {
    interrupt_descripter_table[gate_number].offset_1   = (uint32_t)handler_address & 0xffff;
    interrupt_descripter_table[gate_number].offset_2   = ((uint32_t)handler_address >> 16) & 0xffff;
    interrupt_descripter_table[gate_number].cs_selector = 0x10;
    interrupt_descripter_table[gate_number].zero        = 0;
    interrupt_descripter_table[gate_number].attributes  = 0x8E;
}

void idt_flush(struct idt_descriptor *idt) {
    asm("lidt %0\n" : : "m"(*idt) :);
}

void pic_init() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 32);   outb(0xA1, 40);
    outb(0x21, 4);    outb(0xA1, 2);
    outb(0x21, 1);    outb(0xA1, 1);
    outb(0x21, 0xFC); outb(0xA1, 0xFF);
}

// ============ MAIN ============

void main(void) {
    register uint32_t mb_info_addr asm("ebx");
    uint32_t saved_mb = mb_info_addr;

    graphics_init(saved_mb);

    // ===== PAGING SETUP (required before drawing to framebuffer) =====
    struct ppage tmp;
    tmp.next = (struct ppage *)0;

    uint32_t addr = 0x100000;
    uint32_t end  = ((uint32_t)&_end_kernel + 0xFFF) & ~0xFFF;
    while (addr < end) {
        tmp.physical_addr = (void *)addr;
        map_pages((void *)addr, &tmp, kernel_pd);
        addr += 0x1000;
    }
    for (uint32_t a = 0; a < 0x100000; a += 0x1000) {
        tmp.physical_addr = (void *)a;
        map_pages((void *)a, &tmp, kernel_pd);
    }
    tmp.physical_addr = (void *)0xB8000;
    map_pages((void *)0xB8000, &tmp, kernel_pd);

    uint32_t fb_phys = 0xFD000000;
    for (uint32_t a = fb_phys; a < fb_phys + 640*480*4; a += 0x1000) {
        tmp.physical_addr = (void *)a;
        map_pages((void *)a, &tmp, kernel_pd);
    }

    loadPageDirectory(kernel_pd);
    enablePaging();
    // ===== END PAGING =====

    // <<< NEW: initialize and display the game
    game_init(0xDEADBEEF);

    // ===== INTERRUPT SETUP =====
    for (int k = 0; k < 256; k++)
        set_idt_gate(dummy_handler, k);

    set_idt_gate(keyboard_handler, 33);
    set_idt_gate(pit_handler, 32);

    struct idt_descriptor idt_desc;
    idt_desc.limit = sizeof(interrupt_descripter_table) - 1;
    idt_desc.base  = (uint32_t)interrupt_descripter_table;

    idt_flush(&idt_desc);
    pic_init();
    asm("sti");

    while (1) { asm("hlt"); }
}
