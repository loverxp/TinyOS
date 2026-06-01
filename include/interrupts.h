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
void isr_handler(uint32_t int_no, uint32_t err_code);

// Common IRQ handler
void irq_handler(uint32_t irq_no);

// Load IDT (assembly function)
struct idt_ptr;
extern void idt_load(struct idt_ptr* ptr);

#endif // INTERRUPTS_H
