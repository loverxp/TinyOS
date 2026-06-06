#include "../include/interrupts.h"
#include "../include/io.h"
#include "../include/vga.h"
#include "../include/string.h"
#include "../include/except.h"
#include "../include/scheduler.h"
#include "../include/ipc.h"
#include "../include/rtc.h"
#include "../include/pmm.h"
#include "../include/prng.h"
#include "../include/fat16.h"
#include "../include/debug.h"

// External user-mode exit handlers (defined in user.asm)
extern void forked_task_exit_handler(void);
extern void user_exit_handler(void);

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

extern void isr128(void); // Syscall interrupt (int 0x80)

void (*isr_handlers[IDT_ENTRIES])(void);

static const char* exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved"
};

static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) serial_write(*s++);
}

static void serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        serial_write(hex[(n >> i) & 0xF]);
    }
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

extern void idt_load(struct idt_ptr* ptr);

void idt_initialize(void) {
    idtp.limit = sizeof(struct idt_entry) * IDT_ENTRIES - 1;
    idtp.base = (uint32_t)&idt;
    
    memset(&idt, 0, sizeof(struct idt_entry) * IDT_ENTRIES);
    
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);   idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);   idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);   idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);   idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);   idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E); idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E); idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E); idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E); idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E); idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E); idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E); idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E); idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E); idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E); idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E); idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    
    // IRQ handlers - DPL=3 so they can fire from user mode (Ring 3)
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0xEE);  idt_set_gate(33, (uint32_t)irq1, 0x08, 0xEE);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0xEE);  idt_set_gate(35, (uint32_t)irq3, 0x08, 0xEE);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0xEE);  idt_set_gate(37, (uint32_t)irq5, 0x08, 0xEE);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0xEE);  idt_set_gate(39, (uint32_t)irq7, 0x08, 0xEE);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0xEE);  idt_set_gate(41, (uint32_t)irq9, 0x08, 0xEE);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0xEE); idt_set_gate(43, (uint32_t)irq11, 0x08, 0xEE);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0xEE); idt_set_gate(45, (uint32_t)irq13, 0x08, 0xEE);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0xEE); idt_set_gate(47, (uint32_t)irq15, 0x08, 0xEE);

    // Syscall interrupt - DPL=3 so user mode can trigger it (0xEE = present, ring 3, interrupt gate)
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0xEE);

    idt_load(&idtp);
}

void pic_initialize(void) {
    // ICW1
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    
    // ICW2
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    
    // ICW3
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    
    // ICW4
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Mask all interrupts initially - we'll unmask timer and keyboard later
    // Bit 2 = 0 to unmask IRQ 2 (PIC cascade), needed for slave PIC (IRQ 8-15)
    outb(0x21, 0xFB);  // 0xFB = 11111011: all masked except cascade
    outb(0xA1, 0xFF);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

extern void user_exit_handler(void);

void isr_handler(uint32_t int_no, uint32_t err_code, uint32_t* regs) {
    if (int_no < 32) {
        // Log exception details to serial port for debugging
        serial_string("EXCEPTION: int_no=");
        serial_hex(int_no);
        serial_string(" err=");
        serial_hex(err_code);
        serial_string(" EIP=");
        serial_hex(regs[11]);
        serial_string(" CS=");
        serial_hex(regs[12]);
        // Log first 4 bytes at EIP (the faulting instruction)
        serial_string(" bytes=");
        uint8_t* eip_bytes = (uint8_t*)regs[11];
        for (int i = 0; i < 4; i++) {
            serial_hex(eip_bytes[i]);
            serial_string(" ");
        }
        // Check if from user mode
        uint8_t user_mode = (regs[12] & 0x03) == 3;
        serial_string(user_mode ? " [USER]" : " [KERNEL]");
        serial_string("\r\n");

        exception_handler(int_no, err_code);

        // Check if exception came from user mode (Ring 3)
        // (user_mode already computed above)

        if (int_no == 13 && user_mode) {
            // User-mode GPF: skip the faulting instruction (e.g. hlt = 1 byte)
            regs[11] += 1;  // Increment EIP past the faulting instruction
            vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
            vga_writestring("    Skipped faulty instruction\n");
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        } else if ((int_no == 6 || int_no == 14) && user_mode) {
            // User-mode Invalid Opcode or Page Fault:
            // terminate the user program instead of infinite loop
            serial_string("Terminating user program (exception ");
            serial_hex(int_no);
            serial_string(")\r\n");
            vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
            vga_writestring("    Terminating user program (Invalid Opcode)\n");
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            // Redirect to user_exit_handler to gracefully return to kernel
            regs[11] = (uint32_t)user_exit_handler;  // EIP
            regs[12] = 0x08;                          // CS = kernel code (RPL=0)
        }
    }

    if (isr_handlers[int_no] != 0) {
        isr_handlers[int_no]();
    }
}

void irq_handler(uint32_t irq_no) {
    if (isr_handlers[irq_no + 32] != 0) {
        isr_handlers[irq_no + 32]();
    }
    
    pic_send_eoi(irq_no);
}

void register_interrupt_handler(uint8_t n, void (*handler)(void)) {
    isr_handlers[n] = handler;
}

// Syscall handler (int 0x80)
// regs[0]=ds, [1]=edi, [2]=esi, [3]=ebp, [4]=esp, [5]=ebx, [6]=edx, [7]=ecx, [8]=eax(syno), [9]=int_no, [10]=err_code
// IRET frame at regs[11]=EIP, [12]=CS, [13]=EFLAGS, [14]=ESP, [15]=SS

// Forward declarations for syscall helpers
extern uint32_t timer_get_ticks(void);
extern uint32_t keyboard_read_key(void);
extern char keyboard_getchar(void);
extern void keyboard_clear_buffer(void);
extern void task_yield(void);
extern void task_sleep(uint32_t ms);
extern int task_fork(uint32_t* regs);
extern int pipe_create(void);
extern int pipe_read(int id, void* buf, uint32_t max_len);
extern int pipe_write(int id, const void* data, uint32_t len);
extern void pipe_close(int id);
extern int mq_create(void);
extern int mq_send(int id, const void* data, uint32_t len);
extern int mq_recv(int id, void* buf, uint32_t max_len);
extern void mq_close(int id);
extern void* shm_create(const char* name, uint32_t size);
extern void* shm_open(const char* name);
extern int shm_close(const char* name);

// ── VGA Font Save/Restore ─────────────────────────────────────────────
// Mode 13h (chain-4) writing to 0xA0000 corrupts the font data in VGA plane 2.
// We save the font at boot time and restore it when switching back to text mode.

#define FONT_SIZE 4096  // 256 chars × 16 bytes each (8x16 font)
static uint8_t vga_font_buf[FONT_SIZE];

const uint8_t* vga_get_font(void) {
    return vga_font_buf;
}

void vga_save_font(void) {
    uint8_t old_seq02, old_seq04;
    uint8_t old_gc04, old_gc05, old_gc06;

    // Save registers we'll modify
    outb(0x3C4, 0x02); old_seq02 = inb(0x3C5);  // Map Mask
    outb(0x3C4, 0x04); old_seq04 = inb(0x3C5);  // Memory Mode
    outb(0x3CE, 0x04); old_gc04 = inb(0x3CF);   // Read Map Select
    outb(0x3CE, 0x05); old_gc05 = inb(0x3CF);   // Mode
    outb(0x3CE, 0x06); old_gc06 = inb(0x3CF);   // Misc

    // Configure for reading plane 2 at A0000
    outb(0x3C4, 0x04); outb(0x3C5, 0x06);  // Memory Mode: no chain-4, no odd/even
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);  // Map Mask: plane 2 only
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);  // Read Map Select: plane 2
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);  // Mode: no odd/even
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);  // Misc: A0000, graphics mode

    // Read font from VGA plane 2
    volatile uint8_t* font_vram = (volatile uint8_t*)0xA0000;
    for (int i = 0; i < FONT_SIZE; i++) {
        vga_font_buf[i] = font_vram[i];
    }

    // Restore registers
    outb(0x3C4, 0x02); outb(0x3C5, old_seq02);
    outb(0x3C4, 0x04); outb(0x3C5, old_seq04);
    outb(0x3CE, 0x04); outb(0x3CF, old_gc04);
    outb(0x3CE, 0x05); outb(0x3CF, old_gc05);
    outb(0x3CE, 0x06); outb(0x3CF, old_gc06);

    serial_string("[FONT] Saved ");
    serial_hex(FONT_SIZE);
    serial_string(" bytes of font data\r\n");
}

static void vga_restore_font(void) {
    uint8_t old_seq02, old_seq04;
    uint8_t old_gc05, old_gc06;

    // Save registers we'll modify
    outb(0x3C4, 0x02); old_seq02 = inb(0x3C5);
    outb(0x3C4, 0x04); old_seq04 = inb(0x3C5);
    outb(0x3CE, 0x05); old_gc05 = inb(0x3CF);
    outb(0x3CE, 0x06); old_gc06 = inb(0x3CF);

    // Configure for writing to plane 2 at A0000
    outb(0x3C4, 0x04); outb(0x3C5, 0x06);  // Memory Mode: no chain-4, no odd/even
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);  // Map Mask: plane 2 only
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);  // Mode: no odd/even
    outb(0x3CE, 0x06); outb(0x3CF, 0x05);  // Misc: A0000, graphics mode
    outb(0x3CE, 0x00); outb(0x3CF, 0x00);  // Set/Reset: none
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);  // Enable Set/Reset: none
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);  // Bit Mask: all bits

    // Write font back to plane 2
    volatile uint8_t* font_vram = (volatile uint8_t*)0xA0000;
    for (int i = 0; i < FONT_SIZE; i++) {
        font_vram[i] = vga_font_buf[i];
    }

    // Restore registers
    outb(0x3C4, 0x02); outb(0x3C5, old_seq02);
    outb(0x3C4, 0x04); outb(0x3C5, old_seq04);
    outb(0x3CE, 0x05); outb(0x3CF, old_gc05);
    outb(0x3CE, 0x06); outb(0x3CF, old_gc06);
}

// VGA Mode 13h helpers
static void vga_set_mode13h(void) {
    // Set VGA mode 13h: 320x200, 256 colors, chain-4 linear framebuffer
    //
    // Correct register programming order (critical for proper initialization):
    //   1. Misc Output Register  (set dot clock BEFORE CRTC timing)
    //   2. Sequencer             (reset → program chain-4 → release)
    //   3. Graphics Controller   (256-color mode, A0000 mapping)
    //   4. CRTC                  (unlock → timing params → lock)
    //   5. Attribute Controller  (graphics mode, re-enable video output)
    //   6. DAC Palette           (color table)
    
    // ── 1. Misc Output Register (0x3C2) ──
    // Must write BEFORE CRTC, because CRTC timing values assume
    // the dot clock set here (25.175 MHz for 320-pixel modes).
    outb(0x3C2, 0x63);
    
    // ── 2. Sequencer (port 0x3C4/0x3C5) ──
    // Assert synchronous reset
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    // Program sequencer registers while held in reset
    outb(0x3C4, 0x01); outb(0x3C5, 0x01); // Clocking Mode: 8-dot clock
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F); // Map Mask: all 4 planes
    outb(0x3C4, 0x03); outb(0x3C5, 0x00); // Character Map Select
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E); // Memory Mode: chain-4, ext mem
    // Release reset
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    
    // ── 3. Graphics Controller (port 0x3CE/0x3CF) ──
    outb(0x3CE, 0x00); outb(0x3CF, 0x00);
    outb(0x3CE, 0x01); outb(0x3CF, 0x00);
    outb(0x3CE, 0x02); outb(0x3CF, 0x00);
    outb(0x3CE, 0x03); outb(0x3CF, 0x00);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x40); // Mode: 256-color
    outb(0x3CE, 0x06); outb(0x3CF, 0x05); // Misc: A0000 mapping, graphics
    outb(0x3CE, 0x07); outb(0x3CF, 0x0F);
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF);
    
    // ── 4. CRT Controller (port 0x3D4/0x3D5) ──
    // Unlock CRTC registers 0-7
    outb(0x3D4, 0x11);
    outb(0x3D5, inb(0x3D5) & 0x7F);
    
    outb(0x3D4, 0x00); outb(0x3D5, 0x5F); // Horizontal Total
    outb(0x3D4, 0x01); outb(0x3D5, 0x4F); // Horizontal Display End
    outb(0x3D4, 0x02); outb(0x3D5, 0x50); // Start Horizontal Blank
    outb(0x3D4, 0x03); outb(0x3D5, 0x82); // End Horizontal Blank
    outb(0x3D4, 0x04); outb(0x3D5, 0x54); // Start H Retrace
    outb(0x3D4, 0x05); outb(0x3D5, 0x80); // End H Retrace
    outb(0x3D4, 0x06); outb(0x3D5, 0xBF); // Vertical Total
    outb(0x3D4, 0x07); outb(0x3D5, 0x1F); // Overflow
    outb(0x3D4, 0x08); outb(0x3D5, 0x00); // Preset Row Scan
    outb(0x3D4, 0x09); outb(0x3D5, 0x41); // Max Scan Line
    outb(0x3D4, 0x0A); outb(0x3D5, 0x00); // Cursor Start
    outb(0x3D4, 0x0B); outb(0x3D5, 0x00); // Cursor End
    outb(0x3D4, 0x0C); outb(0x3D5, 0x00); // Start Addr High
    outb(0x3D4, 0x0D); outb(0x3D5, 0x00); // Start Addr Low
    outb(0x3D4, 0x0E); outb(0x3D5, 0x00); // Cursor Loc High
    outb(0x3D4, 0x0F); outb(0x3D5, 0x00); // Cursor Loc Low
    outb(0x3D4, 0x10); outb(0x3D5, 0x9C); // V Retrace Start
    outb(0x3D4, 0x11); outb(0x3D5, 0x8E); // V Retrace End (re-locked)
    outb(0x3D4, 0x12); outb(0x3D5, 0x8F); // V Display End
    outb(0x3D4, 0x13); outb(0x3D5, 0x28); // Offset (40 words)
    outb(0x3D4, 0x14); outb(0x3D5, 0x40); // Underline Loc
    outb(0x3D4, 0x15); outb(0x3D5, 0x96); // Start V Blank
    outb(0x3D4, 0x16); outb(0x3D5, 0xB9); // End V Blank
    outb(0x3D4, 0x17); outb(0x3D5, 0xA3); // CRTC Mode Control
    outb(0x3D4, 0x18); outb(0x3D5, 0xFF); // Line Compare
    
    // ── 5. Attribute Controller (port 0x3C0) ──
    // Reset flip-flop so next write to 0x3C0 is INDEX
    inb(0x3DA);
    // During programming, each index write has bit5=0, which disables video output.
    // This is intentional — prevents display glitches while registers are changing.
    outb(0x3C0, 0x10); outb(0x3C0, 0x41); // Mode Control: graphics, 256-color
    outb(0x3C0, 0x11); outb(0x3C0, 0x00); // Overscan
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F); // Color Plane Enable: all 4 planes
    outb(0x3C0, 0x13); outb(0x3C0, 0x00); // Horizontal PEL Panning
    outb(0x3C0, 0x14); outb(0x3C0, 0x00); // Color Select
    // CRITICAL: After programming, write 0x20 to re-enable video output.
    // The flip-flop is currently in INDEX state (even number of writes),
    // and 0x20 sets bit5=1 which re-enables the display.
    outb(0x3C0, 0x20);
    
    // ── 6. DAC Palette (port 0x3C8/0x3C9) ──
    {
        static const uint8_t std_palette[16][3] = {
            {0x00,0x00,0x00}, {0x00,0x00,0x2A}, {0x00,0x2A,0x00}, {0x00,0x2A,0x2A},
            {0x2A,0x00,0x00}, {0x2A,0x00,0x2A}, {0x2A,0x15,0x00}, {0x2A,0x2A,0x2A},
            {0x15,0x15,0x15}, {0x15,0x15,0x3F}, {0x15,0x3F,0x15}, {0x15,0x3F,0x3F},
            {0x3F,0x15,0x15}, {0x3F,0x15,0x3F}, {0x3F,0x3F,0x15}, {0x3F,0x3F,0x3F},
        };
        outb(0x3C8, 0);
        for (int i = 0; i < 16; i++) {
            outb(0x3C9, std_palette[i][0]);
            outb(0x3C9, std_palette[i][1]);
            outb(0x3C9, std_palette[i][2]);
        }
        for (int i = 16; i < 64; i++) {
            int c = (i - 16) / 16;
            int v = ((i - 16) % 16) * 4;
            outb(0x3C9, (uint8_t)(c == 0 ? v : 0));
            outb(0x3C9, (uint8_t)(c == 1 ? v : 0));
            outb(0x3C9, (uint8_t)(c == 2 ? v : 0));
        }
    }
    
    // ── 7. Write VRAM test pattern ──
    uint8_t* vram = (uint8_t*)0xA0000;
    for (int i = 0; i < 320 * 200; i++) {
        vram[i] = 10;  // bright green background
    }
    for (int i = 0; i < 200; i++) {
        vram[i * 320 + i] = 12;          // red diagonal
        vram[i * 320 + (319 - i)] = 12;   // red diagonal
        vram[i * 320 + 160] = 15;         // white vertical center
    }
    for (int x = 0; x < 320; x++) {
        vram[100 * 320 + x] = 15;         // white horizontal line
    }
    serial_string("[DBG:SYS4] kernel VRAM test pattern drawn\r\n");
}

void vga_set_mode03h(void) {
    // Restore text mode 3 (80x25, 16 colors)
    //
    // Correct register programming order:
    //   1. Misc Output       (set dot clock FIRST — CRTC timing depends on it)
    //   2. Sequencer         (reset → program → release)
    //   3. Graphics Controller
    //   4. CRTC              (unlock → program → lock)
    //   5. Attribute Ctrl    (program with video disabled → re-enable LAST)
    
    // ── 1. Misc Output Register ──
    // 0x67 = 28.322 MHz dot clock, negative H-sync, positive V-sync, color card
    outb(0x3C2, 0x67);
    
    // ── 2. Sequencer ──
    // Assert synchronous reset
    outb(0x3C4, 0x00); outb(0x3C5, 0x01);
    // Program while in reset
    outb(0x3C4, 0x01); outb(0x3C5, 0x00); // Clocking Mode: 9-dot clock
    outb(0x3C4, 0x02); outb(0x3C5, 0x03); // Map Mask: planes 0+1 (text uses 2 planes)
    outb(0x3C4, 0x03); outb(0x3C5, 0x00); // Character Map Select
    outb(0x3C4, 0x04); outb(0x3C5, 0x03); // Memory Mode: ext mem, odd/even, no chain-4
    // Release reset
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    
    // ── 3. Graphics Controller ──
    outb(0x3CE, 0x00); outb(0x3CF, 0x00); // Set/Reset
    outb(0x3CE, 0x01); outb(0x3CF, 0x00); // Enable Set/Reset
    outb(0x3CE, 0x02); outb(0x3CF, 0x00); // Color Compare
    outb(0x3CE, 0x03); outb(0x3CF, 0x00); // Data Rotate
    outb(0x3CE, 0x04); outb(0x3CF, 0x00); // Read Map Select
    outb(0x3CE, 0x05); outb(0x3CF, 0x10); // Mode: odd/even, no 256-color
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E); // Misc: B8000, text mode, odd/even
    outb(0x3CE, 0x07); outb(0x3CF, 0x00); // Color Don't Care
    outb(0x3CE, 0x08); outb(0x3CF, 0xFF); // Bit Mask
    
    // ── 4. CRTC ──
    // Unlock CRTC registers 0-7
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & 0x7F);
    
    outb(0x3D4, 0x00); outb(0x3D5, 0x5F); // Horizontal Total
    outb(0x3D4, 0x01); outb(0x3D5, 0x4F); // Horizontal Display End
    outb(0x3D4, 0x02); outb(0x3D5, 0x50); // Start Horizontal Blank
    outb(0x3D4, 0x03); outb(0x3D5, 0x82); // End Horizontal Blank
    outb(0x3D4, 0x04); outb(0x3D5, 0x55); // Start H Retrace
    outb(0x3D4, 0x05); outb(0x3D5, 0x81); // End H Retrace
    outb(0x3D4, 0x06); outb(0x3D5, 0xBF); // Vertical Total
    outb(0x3D4, 0x07); outb(0x3D5, 0x1F); // Overflow
    outb(0x3D4, 0x08); outb(0x3D5, 0x00); // Preset Row Scan
    outb(0x3D4, 0x09); outb(0x3D5, 0x4F); // Max Scan Line (16 scan lines)
    outb(0x3D4, 0x0A); outb(0x3D5, 0x0E); // Cursor Start
    outb(0x3D4, 0x0B); outb(0x3D5, 0x0F); // Cursor End
    outb(0x3D4, 0x0C); outb(0x3D5, 0x00); // Start Addr High
    outb(0x3D4, 0x0D); outb(0x3D5, 0x00); // Start Addr Low
    outb(0x3D4, 0x0E); outb(0x3D5, 0x00); // Cursor Loc High
    outb(0x3D4, 0x0F); outb(0x3D5, 0x00); // Cursor Loc Low
    outb(0x3D4, 0x10); outb(0x3D5, 0x9C); // V Retrace Start
    outb(0x3D4, 0x11); outb(0x3D5, 0x8E); // V Retrace End (re-locked)
    outb(0x3D4, 0x12); outb(0x3D5, 0x8F); // V Display End
    outb(0x3D4, 0x13); outb(0x3D5, 0x28); // Offset (40 words)
    outb(0x3D4, 0x14); outb(0x3D5, 0x1F); // Underline Loc
    outb(0x3D4, 0x15); outb(0x3D5, 0x96); // Start V Blank
    outb(0x3D4, 0x16); outb(0x3D5, 0xB9); // End V Blank
    outb(0x3D4, 0x17); outb(0x3D5, 0xA3); // CRTC Mode Control
    outb(0x3D4, 0x18); outb(0x3D5, 0xFF); // Line Compare
    
    // ── 5. Attribute Controller (0x3C0) ──
    // Reset flip-flop to INDEX state
    inb(0x3DA);
    
    // Program Attribute Controller palette registers (0x00-0x0F).
    // These map 4-bit text attribute colors to DAC indices.
    // After Mode 13h, these may be in an undefined state, so we set
    // an identity mapping: color i → DAC index i.
    for (int i = 0; i < 16; i++) {
        outb(0x3C0, i);       // Palette register index
        outb(0x3C0, i);       // Value = DAC index (identity)
    }
    
    // Program control registers — during this, video is disabled (bit5=0)
    outb(0x3C0, 0x10); outb(0x3C0, 0x0C); // Mode Control: text, color, 9-dot
    outb(0x3C0, 0x11); outb(0x3C0, 0x00); // Overscan
    outb(0x3C0, 0x12); outb(0x3C0, 0x0F); // Color Plane Enable: all
    outb(0x3C0, 0x13); outb(0x3C0, 0x08); // Horizontal PEL Panning
    outb(0x3C0, 0x14); outb(0x3C0, 0x00); // Color Select
    // CRITICAL: Re-enable video output NOW (all registers have been set)
    outb(0x3C0, 0x20);
    
    // ── 6. Restore font data to VGA plane 2 ──
    // Mode 13h (chain-4) writing to 0xA0000 corrupts font data.
    // We saved it at boot and must restore it here.
    vga_restore_font();
    
    // ── 7. Clear text screen ──
    // Note: Not clearing here — the caller should decide whether to clear.
    // Clearing here would destroy output written by user programs (e.g. hello via syscall 7).
}

// Kernel-mode VGA graphics test — bypasses user mode entirely.
// Called from shell command "gtest" for direct hardware debugging.
void vga_gfx_test(void) {
    serial_string("[GTEST] Kernel-mode VGA test starting...\r\n");
    
    // Switch to Mode 13h and draw test pattern
    vga_set_mode13h();
    serial_string("[GTEST] Mode 13h set, test pattern drawn. Press any key to return...\r\n");
    
    // Wait for any keypress (poll keyboard port directly)
    while ((inb(0x64) & 1) == 0) {
        asm volatile("hlt");
    }
    inb(0x60); // ack the key
    
    // Restore text mode
    vga_set_mode03h();
    serial_string("[GTEST] Text mode restored.\r\n");
}

void syscall_handler(uint32_t* regs) {
    uint32_t syscall_no = regs[8];  // EAX contains syscall number
    uint32_t arg1 = regs[5];        // EBX
    uint32_t arg2 = regs[6];        // ECX (moved to EDX in syscall convention?)
    // Actually in our ISR stub: pusha saves: edi, esi, ebp, esp, ebx, edx, ecx, eax
    // So regs[5]=ebx, regs[6]=edx, regs[7]=ecx
    // Let's use ebx for first arg, ecx for second arg
    arg2 = regs[7]; // ECX

    if (syscall_no == 0) {
        // Syscall 0: exit user mode, return to kernel
        task_t* cur_task = scheduler_get_current();

        // Forked tasks should terminate properly via task_exit()
        if (cur_task && cur_task->is_forked) {
            serial_string("[DBG:SYS0] forked task '");
            serial_string(cur_task->name);
            serial_string("' exited, code=");
            serial_hex(arg1);
            serial_string("\r\n");

            // Return to kernel mode at forked_task_exit_handler which calls task_exit()
            regs[11] = (uint32_t)forked_task_exit_handler;  // EIP
            regs[12] = 0x08;                                  // CS = kernel code (RPL=0)
            return;
        }

        // Root task: switch back to text mode and return to kernel
        vga_set_mode03h();

        serial_string("[DBG:SYS0] user_exit called, code=");
        serial_hex(arg1);
        serial_string("\r\n");
        vga_writestring("\n*** Return to kernel mode ***\n\n");

        // Modify the saved IRET frame to return to kernel code
        regs[11] = (uint32_t)user_exit_handler;  // EIP
        regs[12] = 0x08;                          // CS = kernel code (RPL=0)
        return;
    }

    if (syscall_no == 1) {
        // Syscall 1: print message (legacy)
        vga_writestring("[Syscall] #1 from user mode\n");
        return;
    }

    if (syscall_no == 2) {
        // Syscall 2: get timer ticks -> return in EAX
        regs[8] = timer_get_ticks();
        return;
    }

    if (syscall_no == 3) {
        // Syscall 3: read key (non-blocking) -> return scancode in EAX
        // High bit set if extended (0xE0 prefix)
        regs[8] = keyboard_read_key();
        return;
    }

    if (syscall_no == 4) {
        // Syscall 4: set video mode
        // arg1: 0 = text mode (03h), 1 = graphics mode (13h)
        serial_string("[DBG:SYS4] set_video_mode: ");
        serial_hex(arg1);
        serial_string("\r\n");
        if (arg1 == 1) {
            serial_string("[DBG:SYS4] entering vga_set_mode13h...\r\n");
            vga_set_mode13h();
            serial_string("[DBG:SYS4] vga_set_mode13h done\r\n");
        } else {
            vga_set_mode03h();
        }
        return;
    }

    if (syscall_no == 5) {
        // Syscall 5: clear keyboard buffer
        keyboard_clear_buffer();
        return;
    }

    if (syscall_no == 6) {
        // Syscall 6: debug print to serial port (arg1 = string pointer)
        const char* s = (const char*)arg1;
        while (*s) {
            while ((inb(0x3FD) & 0x20) == 0);
            outb(0x3F8, *s++);
        }
        return;
    }

    if (syscall_no == 7) {
        // Syscall 7: write string to VGA console (arg1 = string pointer)
        // If current task has stdout_pipe, redirect to pipe instead
        const char* s = (const char*)arg1;
        if (s) {
            task_t* cur = scheduler_get_current();
            if (cur && cur->stdout_pipe >= 0) {
                pipe_write(cur->stdout_pipe, s, strlen(s));
            } else {
                vga_writestring(s);
            }
        }
        return;
    }

    if (syscall_no == 8) {
        // Syscall 8: yield CPU to other tasks
        task_yield();
        return;
    }

    if (syscall_no == 9) {
        // Syscall 9: sleep for N milliseconds (arg1 = ms)
        task_sleep(arg1);
        return;
    }

    if (syscall_no == 25) {
        // Syscall 25: fork() - clone the current task
        // Returns child PID in parent, 0 in child
        regs[8] = (uint32_t)task_fork(regs);
        return;
    }

    if (syscall_no == 26) {
        // Syscall 26: exec(entry, user_esp) - replace user program
        // arg1 = new entry point (EIP)
        // arg2 = new user stack pointer (ESP)
        regs[8] = 0;            // EAX = 0 (success)
        regs[11] = arg1;        // EIP = entry point
        regs[13] = 0x3200;      // EFLAGS: IF=1, IOPL=3
        regs[14] = arg2;        // user ESP
        // Clear other user registers
        regs[1] = 0;  // EDI
        regs[2] = 0;  // ESI
        regs[3] = 0;  // EBP
        regs[4] = 0;  // ESP (saved by pusha)
        regs[5] = 0;  // EBX
        regs[6] = 0;  // EDX
        regs[7] = 0;  // ECX
        return;
    }

    uint32_t arg3 = regs[6]; // EDX

    if (syscall_no == 10) {
        regs[8] = (uint32_t)pipe_create();
        return;
    }
    if (syscall_no == 11) {
        regs[8] = (uint32_t)pipe_read((int)arg1, (void*)arg2, arg3);
        return;
    }
    if (syscall_no == 12) {
        regs[8] = (uint32_t)pipe_write((int)arg1, (const void*)arg2, arg3);
        return;
    }
    if (syscall_no == 13) {
        pipe_close((int)arg1);
        return;
    }
    if (syscall_no == 14) {
        regs[8] = (uint32_t)mq_create();
        return;
    }
    if (syscall_no == 15) {
        regs[8] = (uint32_t)mq_send((int)arg1, (const void*)arg2, arg3);
        return;
    }
    if (syscall_no == 16) {
        regs[8] = (uint32_t)mq_recv((int)arg1, (void*)arg2, arg3);
        return;
    }
    if (syscall_no == 17) {
        mq_close((int)arg1);
        return;
    }
    if (syscall_no == 18) {
        regs[8] = (uint32_t)shm_create((const char*)arg1, arg2);
        return;
    }
    if (syscall_no == 19) {
        regs[8] = (uint32_t)shm_open((const char*)arg1);
        return;
    }
    if (syscall_no == 20) {
        regs[8] = (uint32_t)shm_close((const char*)arg1);
        return;
    }

    if (syscall_no == 21) {
        // Syscall 21: getchar() - blocking read character
        // If stdin_pipe is set, read from pipe instead of keyboard
        task_t* cur = scheduler_get_current();
        if (cur && cur->stdin_pipe >= 0) {
            char c;
            if (pipe_read(cur->stdin_pipe, &c, 1) == 1) {
                regs[8] = (uint32_t)(unsigned char)c;
            } else {
                regs[8] = 0;  // EOF
            }
        } else {
            regs[8] = (uint32_t)(unsigned char)keyboard_getchar();
        }
        return;
    }

    if (syscall_no == 22) {
        // Syscall 22: readline(buf, max) - blocking read a line
        // arg1 = buffer address, arg2 = max length
        char* buf = (char*)arg1;
        int max = (int)arg2;
        int pos = 0;

        task_t* cur = scheduler_get_current();
        if (cur && cur->stdin_pipe >= 0) {
            /* Read from pipe until newline or max */
            while (pos < max - 1) {
                char c;
                if (pipe_read(cur->stdin_pipe, &c, 1) != 1) break;
                if (c == '\n' || c == '\r') {
                    buf[pos] = '\0';
                    break;
                }
                if (c >= 32) buf[pos++] = c;
            }
            buf[pos] = '\0';
            regs[8] = (uint32_t)pos;
        } else {
            while (pos < max - 1) {
                char c = keyboard_getchar();
                if (c == '\b') {
                    if (pos > 0) { pos--; }
                    continue;
                }
                if (c == '\n' || c == '\r') {
                    buf[pos] = '\0';
                    break;
                }
                if (c >= 32) {
                    buf[pos++] = c;
                }
            }
            buf[pos] = '\0';
            regs[8] = (uint32_t)pos;
        }
        return;
    }

    if (syscall_no == 23) {
        // Syscall 23: get_cmdline(buf, max) - get command line args for user cmd
        // arg1 = buffer address, arg2 = max length
        extern char user_cmd_args[256];
        char* dst = (char*)arg1;
        uint32_t max = arg2;
        uint32_t i;
        for (i = 0; i < max - 1 && user_cmd_args[i]; i++) {
            dst[i] = user_cmd_args[i];
        }
        dst[i] = '\0';
        regs[8] = i;
        return;
    }

    if (syscall_no == 24) {
        // Syscall 24: clear_screen() - clear VGA text screen
        vga_clear_screen(VGA_COLOR_BLACK);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        return;
    }

    if (syscall_no == 27) {
        // Syscall 27: get_system_info(type, buffer, max_len)
        // type: 0=uptime, 1=date, 2=rand, 3=meminfo, 4=diskinfo
        // Returns bytes written to buffer, or 0 on failure
        uint32_t info_type = arg1;
        void* buf = (void*)arg2;
        uint32_t max = arg3;

        /* Debug: print syscall params */
        KDBG("sys27", "type=%u buf=0x%x max=%u", info_type, (uint32_t)buf, max);

        if (!buf || max == 0) { regs[8] = 0; return; }

        if (info_type == 0) {
            /* Uptime: return ticks (uint32_t) */
            if (max < sizeof(uint32_t)) { regs[8] = 0; return; }
            uint32_t ticks = timer_get_ticks();
            *(uint32_t*)buf = ticks;
            regs[8] = sizeof(uint32_t);
        } else if (info_type == 1) {
            /* Date/time: return rtc_time_t */
            if (max < sizeof(rtc_time_t)) { regs[8] = 0; return; }
            rtc_read_time((rtc_time_t*)buf);
            regs[8] = sizeof(rtc_time_t);
        } else if (info_type == 2) {
            /* Random number: seed with ticks, return uint32_t */
            if (max < sizeof(uint32_t)) { regs[8] = 0; return; }
            prng_seed(timer_get_ticks());
            *(uint32_t*)buf = prng_next();
            regs[8] = sizeof(uint32_t);
        } else if (info_type == 3) {
            /* Memory info: total_pages, free_pages, used_pages, total_kb */
            uint32_t meminfo[4];
            meminfo[0] = pmm_get_total_pages();
            meminfo[1] = pmm_get_free_pages();
            meminfo[2] = pmm_get_used_pages();
            meminfo[3] = pmm_get_total_memory_kb();
            uint32_t copy_size = max < sizeof(meminfo) ? max : sizeof(meminfo);
            memcpy(buf, meminfo, copy_size);
            regs[8] = copy_size;
        } else if (info_type == 4) {
            /* Disk info: total_size, bytes_per_sector, sectors_per_cluster,
             * total_clusters, root_entry_count */
            const fat16_bpb_t* bpb = fat16_get_bpb();
            if (!bpb) { regs[8] = 0; return; }
            uint32_t dinfo[5];
            dinfo[0] = bpb->total_size;
            dinfo[1] = bpb->bytes_per_sector;
            dinfo[2] = bpb->sectors_per_cluster;
            dinfo[3] = bpb->total_clusters;
            dinfo[4] = bpb->root_entry_count;
            uint32_t copy_size = max < sizeof(dinfo) ? max : sizeof(dinfo);
            memcpy(buf, dinfo, copy_size);
            regs[8] = copy_size;
        } else {
            regs[8] = 0;  /* Unknown type */
        }
        return;
    }

    // Unknown syscall
    vga_writestring("[Syscall] #");
    vga_write_dec(syscall_no);
    vga_writestring(" from user mode (unknown)\n");
}
