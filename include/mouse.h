#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

/* PS/2 mouse packet */
typedef struct {
    int x;              /* Relative X movement */
    int y;              /* Relative Y movement */
    uint8_t buttons;    /* Bit 0=left, 1=right, 2=middle */
} mouse_packet_t;

/* Global mouse state (absolute position + buttons) */
extern int mouse_x;
extern int mouse_y;
extern uint8_t mouse_buttons;

/* Initialize PS/2 mouse on IRQ12 */
void mouse_init(void);

/* Mouse IRQ handler */
void mouse_handler(void);

/* Register a callback for mouse motion/click events.
   The callback receives absolute coordinates and button state. */
typedef void (*mouse_event_cb)(int x, int y, uint8_t buttons);
void mouse_register_callback(mouse_event_cb cb);

/* Get current mouse position */
int mouse_get_x(void);
int mouse_get_y(void);
uint8_t mouse_get_buttons(void);

#endif /* MOUSE_H */