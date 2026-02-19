#include <stdint.h>
#include "page.h"

// NEVER DELETE NEXT TWO LINES OF CODE FOR BOOT TO GRUB
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6

const unsigned int multiboot_header[]
__attribute__((section(".multiboot"))) = {
    MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16 + MULTIBOOT2_HEADER_MAGIC)
};

// CAN MAKE EDITS PAST THIS

// ============ INTERRUPT STRUCTURES ============

struct idt_gate { 
    uint16_t offset_1;      // LS 16bits of handler address 
    uint16_t cs_selector;   // 0x10
    uint8_t zero;           // 0
    uint8_t attributes;     // 0x8E
    uint16_t offset_2;      // MS 16bits of handler address 
};

struct idt_descriptor { // special 386 struct
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

// ============ SCREEN DRIVER ============

int x = 0;
int y = 0;

#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25
#define VIDEO_MEMORY  0xB8000
#define COLOR 0x07

static void scroll(void) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;
    
    for (int row = 1; row < SCREEN_HEIGHT; row++) {
        for (int col = 0; col < SCREEN_WIDTH; col++) {
            int src = (row * SCREEN_WIDTH + col) * 2;
            int dst = ((row - 1) * SCREEN_WIDTH + col) * 2;
            vram[dst]     = vram[src];
            vram[dst + 1] = vram[src + 1];
        }
    }
    
    for (int col = 0; col < SCREEN_WIDTH; col++) {
        int off = ((SCREEN_HEIGHT - 1) * SCREEN_WIDTH + col) * 2;
        vram[off]     = ' ';
        vram[off + 1] = COLOR;
    }
}

void putc(int data) {
    volatile unsigned char *vram = (volatile unsigned char*)VIDEO_MEMORY;
    unsigned char c = (unsigned char)data;
    
    if (c == '\n') {
        x = 0;
        y++;
    } else if (c == '\b') {
        if (x > 0) {
            x--;
            int off = (y * SCREEN_WIDTH + x) * 2;
            vram[off] = ' ';
            vram[off + 1] = COLOR;
        }
    } else if (c == '\t') {
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
    
    if (y >= SCREEN_HEIGHT) {
        scroll();
        y = SCREEN_HEIGHT - 1;
        if (x >= SCREEN_WIDTH) x = 0;
    }
}

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

// ============ INTERRUPT HANDLERS ============

__attribute__((interrupt)) void dummy_handler(void *p) {
    // Do nothing
}

__attribute__((interrupt)) void keyboard_handler(void *p) {
    // Read scancode from keyboard controller
    uint8_t scancode = inb(0x60);
    
    // Translate and print if valid
    if (scancode < 128 && keyboard_map[scancode] != 0) {
        putc(keyboard_map[scancode]);
    }
    
    // Acknowledge interrupt
    outb(0x20, 0x20);
}

__attribute__((interrupt)) void pit_handler(void *p) {
    static unsigned int ticks = 0;
    static unsigned int seconds = 0;

    ticks++;
    if (ticks > 18) {
        ticks = 0;
        seconds++;
        seconds %= 10;
        unsigned char *vram = (unsigned char *)0xb8000;
        // Put counter at top-right corner (column 79, row 0)
        int offset = (0 * SCREEN_WIDTH + 79) * 2;
        vram[offset] = seconds + '0';
        vram[offset + 1] = COLOR;
    }
    
    // Acknowledge interrupt
    outb(0x20, 0x20);
}

// ============ INTERRUPT SETUP FUNCTIONS ============

void set_idt_gate(void *handler_address, unsigned int gate_number) {
    interrupt_descripter_table[gate_number].offset_1 = (uint32_t)handler_address & 0xffff;
    interrupt_descripter_table[gate_number].offset_2 = ((uint32_t)handler_address >> 16) & 0xffff;
    interrupt_descripter_table[gate_number].cs_selector = 0x10;
    interrupt_descripter_table[gate_number].zero = 0;
    interrupt_descripter_table[gate_number].attributes = 0x8E;
}

void idt_flush(struct idt_descriptor *idt) {
    asm("lidt %0\n"
        :
        : "m"(*idt)
        :);
}

void pic_init() {
    outb(0x20, 0x11); // RESET command to primary PIC
    outb(0xA0, 0x11); // RESET command to secondary PIC

    // Remapping the PIC
    outb(0x21, 32);
    outb(0xA1, 40);

    outb(0x21, 4); // Primary PIC cascade interrupts 
    outb(0xA1, 2); // Secondary PIC - "slave" mode

    outb(0x21, 1); // 8086 mode
    outb(0xA1, 1); // 8086 mode

    outb(0x21, 0xFC); // Unmask interrupts (keyboard = IRQ1, timer = IRQ0)
    outb(0xA1, 0xFF); // Mask all secondary PIC interrupts
}

// ============ MAIN ============

void main(void) {
    clear_screen();

    // ===== PAGE ALLOCATOR TEST =====
    init_pfa_list();
    puts("init_pfa_list: PASS\n");

    struct ppage *p = allocate_physical_pages(3);
    if (p != (void *)0) {
        puts("allocate 3 pages: PASS\n");
    } else {
        puts("allocate 3 pages: FAIL\n");
    }

    free_physical_pages(p);
    puts("free_physical_pages: PASS\n");

    struct ppage *p2 = allocate_physical_pages(3);
    if (p2 != (void *)0) {
        puts("re-allocate after free: PASS\n");
    } else {
        puts("re-allocate after free: FAIL\n");
    }

    struct ppage *too_many = allocate_physical_pages(200);
    if (too_many == (void *)0) {
        puts("over-allocate returns NULL: PASS\n");
    } else {
        puts("over-allocate returns NULL: FAIL\n");
    }
    puts("\n");
    // ===== END TEST =====

    puts("Interrupt-driven OS ready!\n");
    puts("Initializing interrupts...\n");
    
    // Set up all interrupt gates
    for (int k = 0; k < 256; k++) {
        set_idt_gate(dummy_handler, k);
    }
    
    // Install our handlers
    set_idt_gate(keyboard_handler, 33);  // IRQ1 = interrupt 33
    set_idt_gate(pit_handler, 32);       // IRQ0 = interrupt 32
    
    // Load the IDT
    struct idt_descriptor idt_desc;
    idt_desc.limit = sizeof(interrupt_descripter_table) - 1;
    idt_desc.base = (uint32_t)interrupt_descripter_table;
    
    idt_flush(&idt_desc);
    
    // Initialize the PIC
    pic_init();
    
    // Enable interrupts!
    asm("sti");
    
    puts("Interrupts enabled! Start typing...\n\n");
    
    // Main loop - just halt and wait for interrupts
    while (1) {
        asm("hlt");
    }
}
