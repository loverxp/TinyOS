#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

// Initialize IDT (Interrupt Descriptor Table)
void idt_initialize(void);

// Remap PIC (Programmable Interrupt Controller)
void pic_initialize(void);

// Unmask (enable) a specific IRQ
void pic_unmask_irq(uint8_t irq);

// Send End of Interrupt signal
void pic_send_eoi(uint8_t irq);

// Register an interrupt handler
void register_interrupt_handler(uint8_t n, void (*handler)(void));

// Common ISR handler
// regs points to the saved register frame: regs[11]=EIP, regs[12]=CS, regs[13]=EFLAGS
void isr_handler(uint32_t int_no, uint32_t err_code, uint32_t* regs);

// Common IRQ handler
void irq_handler(uint32_t irq_no);

// Syscall handler (int 0x80)
void syscall_handler(uint32_t* regs);

// Load IDT (assembly function)
struct idt_ptr;
extern void idt_load(struct idt_ptr* ptr);

// VGA graphics mode test (kernel-mode direct test, no user program needed)
void vga_gfx_test(void);

#endif // INTERRUPTS_H
