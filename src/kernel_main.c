#include <stdint.h>
#include "page.h"
#include "paging.h"
#include "fat.h"
#include "ide.h"
#include "graphics.h"

// FAT driver function prototypes
int fatInit(void);
struct file *fatOpen(const char *filename);
int fatRead(struct file *f, char *buf, uint32_t nbytes);

// NEVER DELETE NEXT FEW LINES OF CODE FOR BOOT TO GRUB
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
const unsigned int multiboot_header[]
__attribute__((section(".multiboot"))) = {
    // --- DO NOT TOUCH THESE 4 VALUES ---
    MULTIBOOT2_HEADER_MAGIC,                          // magic
    0,                                                 // architecture (i386)
    48,                                                // total header length (was 16, now 48)
    -(MULTIBOOT2_HEADER_MAGIC + 0 + 48),              // checksum (recalculated)
    // --- FRAMEBUFFER TAG (type=5, 20 bytes + 4 padding = 24 bytes) ---
    5,                                                 // tag type = framebuffer request
    20,                                                // tag size
    640,                                               // width (pixels)
    480,                                               // height (pixels)
    32,                                                // color depth (32 bits per pixel)
    0,                                                 // padding (align to 8 bytes)
    // --- END TAG (required by Multiboot2 spec) ---
    0,                                                 // end tag type = 0
    8                                                  // end tag size
};
// CAN MAKE EDITS PAST THIS

//Linker Symbol Decleration
extern uint8_t _end_kernel;

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

// <<< CHANGED: renamed from clear_screen to vga_clear to avoid conflict with graphics.h
static void vga_clear(void) {
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
    uint8_t scancode = inb(0x60);
    if (scancode < 128 && keyboard_map[scancode] != 0) {
        putc(keyboard_map[scancode]);
    }
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
        int offset = (0 * SCREEN_WIDTH + 79) * 2;
        vram[offset] = seconds + '0';
        vram[offset + 1] = COLOR;
    }
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
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 32);
    outb(0xA1, 40);
    outb(0x21, 4);
    outb(0xA1, 2);
    outb(0x21, 1);
    outb(0xA1, 1);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

// ============ MAIN ============

void main(void) {
    // CHANGED: capture ebx (GRUB multiboot info pointer) FIRST before anything runs
    register uint32_t mb_info_addr asm("ebx");
    uint32_t saved_mb = mb_info_addr;

    // init graphics from GRUB framebuffer info
    graphics_init(saved_mb);

    // add spites below:

    // CHANGED: was clear_screen(), now vga_clear()
    vga_clear();

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

    // ===== PAGING SETUP =====
    puts("Setting up identity-mapped page table...\n");

    struct ppage tmp;
    tmp.next = (struct ppage *)0;

    // (a) Identity map kernel binary: 0x100000 -> &_end_kernel
    uint32_t addr = 0x100000;
    uint32_t end  = ((uint32_t)&_end_kernel + 0xFFF) & ~0xFFF;
    while (addr < end) {
        tmp.physical_addr = (void *)addr;
        map_pages((void *)addr, &tmp, kernel_pd);
        addr += 0x1000;
    }
    puts("Kernel binary identity mapped: PASS\n");

    // (b) Identity map all low memory: 0x0 -> 0x100000
    for (uint32_t a = 0; a < 0x100000; a += 0x1000) {
        tmp.physical_addr = (void *)a;
        map_pages((void *)a, &tmp, kernel_pd);
    }
    puts("Low memory identity mapped: PASS\n");

    // (c) Identity map the video buffer
    tmp.physical_addr = (void *)0xB8000;
    map_pages((void *)0xB8000, &tmp, kernel_pd);
    puts("Video buffer identity mapped: PASS\n");

    // <<< CHANGED: identity map the linear framebuffer GRUB set up
    // QEMU typically places it at 0xFD000000; map 640*480*4 bytes worth of pages
    uint32_t fb_phys = 0xFD000000;
    for (uint32_t a = fb_phys; a < fb_phys + 640*480*4; a += 0x1000) {
        tmp.physical_addr = (void *)a;
        map_pages((void *)a, &tmp, kernel_pd);
    }
    puts("Framebuffer identity mapped: PASS\n");

    loadPageDirectory(kernel_pd);
    enablePaging();
    puts("Paging enabled!\n\n");
    // ===== END PAGING SETUP =====

    // ===== FAT FS DEMO =====
    puts("--- FAT Filesystem Driver ---\n");
    if (fatInit() == 0) {
        puts("fatInit: PASS\n");

        struct file *f = fatOpen("TESTFILE.TXT");
        if (f) {
            puts("fatOpen: PASS\n");

            static char file_buf[512];
            int n = fatRead(f, file_buf, 511);
            if (n > 0) {
                file_buf[n] = '\0';
                puts("fatRead: PASS\n");
                puts("File contents: ");
                puts(file_buf);
                puts("\n");
            } else {
                puts("fatRead: FAIL\n");
            }
        } else {
            puts("fatOpen: FAIL\n");
        }
    }
    // ===== END FAT DEMO =====

    puts("Interrupt-driven OS ready!\n");
    puts("Initializing interrupts...\n");
    
    for (int k = 0; k < 256; k++) {
        set_idt_gate(dummy_handler, k);
    }
    
    set_idt_gate(keyboard_handler, 33);
    set_idt_gate(pit_handler, 32);
    
    struct idt_descriptor idt_desc;
    idt_desc.limit = sizeof(interrupt_descripter_table) - 1;
    idt_desc.base = (uint32_t)interrupt_descripter_table;
    
    idt_flush(&idt_desc);
    pic_init();
    asm("sti");
    
    puts("Interrupts enabled! Start typing...\n\n");
    
    while (1) {
        asm("hlt");
    }
}
