#include <stdint.h>

/* ---------- Port I/O ---------- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------- Screen (VGA text mode) ---------- */
#define VGA   ((volatile uint16_t *)0xB8000)
#define COLS  80
#define ROWS  25
static int row = 0, col = 0;

static void clear(void) {
    for (int i = 0; i < COLS * ROWS; i++) VGA[i] = (0x0F << 8) | ' ';
    row = col = 0;
}
static void scroll(void) {
    for (int i = 0; i < COLS * (ROWS - 1); i++) VGA[i] = VGA[i + COLS];
    for (int i = COLS * (ROWS - 1); i < COLS * ROWS; i++) VGA[i] = (0x0F << 8) | ' ';
    row = ROWS - 1;
}
static void putc(char c) {
    if (c == '\n') { col = 0; row++; }
    else if (c == '\b') {
        if (col > 0) { col--; VGA[row * COLS + col] = (0x0F << 8) | ' '; }
        return;
    }
    else {
        VGA[row * COLS + col] = (0x0F << 8) | (uint8_t)c;
        if (++col == COLS) { col = 0; row++; }
    }
    if (row == ROWS) scroll();
}
static void print(const char *s) { while (*s) putc(*s++); }

/* ---------- IDT ---------- */
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

extern void idt_load(struct idt_ptr *p);
extern void irq0_stub(void);
extern void irq1_stub(void);

static void idt_set(int n, void (*handler)(void)) {
    uint32_t base = (uint32_t)handler;
    idt[n].base_lo = base & 0xFFFF;
    idt[n].base_hi = (base >> 16) & 0xFFFF;
    idt[n].sel     = 0x08;
    idt[n].always0 = 0;
    idt[n].flags   = 0x8E;
}

/* ---------- PIC (8259) ---------- */
static void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}
static void pic_eoi(void) { outb(0x20, 0x20); }

/* ---------- Timer and keyboard ---------- */
static volatile uint32_t ticks = 0;

static const char keymap[] =
    "\0" "\033" "1234567890" "-=" "\b" "\t" "qwertyuiop" "[]" "\n" "\0"
    "asdfghjkl" ";'`" "\0" "\\" "zxcvbnm" ",./" "\0" "*" "\0" " ";

#define KBUF 64
static volatile char kbuf[KBUF];
static volatile int khead = 0, ktail = 0;

static void kbuf_push(char c) {
    int next = (khead + 1) % KBUF;
    if (next != ktail) { kbuf[khead] = c; khead = next; }
}

void irq_dispatch(uint32_t vector) {
    if (vector == 32) {
        ticks++;
    } else if (vector == 33) {
        uint8_t sc = inb(0x60);
        if (!(sc & 0x80) && sc < sizeof(keymap) && keymap[sc])
            kbuf_push(keymap[sc]);
    }
    pic_eoi();
}

static int kbuf_pop(char *c) {
    if (khead == ktail) return 0;
    *c = kbuf[ktail];
    ktail = (ktail + 1) % KBUF;
    return 1;
}

/* ---------- Kernel entry ---------- */
void kernel_main(void) {
    clear();
    print("PhoneOS Kernel 0.2\n");
    print("Interrupts + keyboard online.\n");
    print("Type something:\n\n> ");

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    idt_set(32, irq0_stub);
    idt_set(33, irq1_stub);
    idt_load(&idtp);

    pic_init();
    __asm__ volatile ("sti");

    char c;
    for (;;) {
        __asm__ volatile ("cli");
        int got = kbuf_pop(&c);
        __asm__ volatile ("sti");
        if (got) {
            putc(c);
            if (c == '\n') print("> ");
        } else {
            __asm__ volatile ("hlt");
        }
    }
}
