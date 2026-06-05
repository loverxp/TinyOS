#ifndef VBE_H
#define VBE_H

#include "types.h"

/* Bochs VBE I/O ports */
#define VBE_DISPI_IOPORT_INDEX  0x01CE
#define VBE_DISPI_IOPORT_DATA   0x01CF

/* VBE register indices */
#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

/* Enable register values */
#define VBE_DISPI_DISABLED      0x00
#define VBE_DISPI_ENABLED       0x01
#define VBE_DISPI_GETCAPS       0x02
#define VBE_DISPI_8BIT_DAC      0x20
#define VBE_DISPI_LFB_ENABLED   0x40
#define VBE_DISPI_NOCLEARMEM    0x80

/* QEMU std VGA PCI identifiers */
#define QEMU_VGA_VENDOR 0x1234
#define QEMU_VGA_DEVICE 0x1111

/* Detect Bochs VBE support (returns 1 if present) */
int vbe_detect(void);

/* Set a display mode via Bochs VBE */
int vbe_set_mode(uint16_t width, uint16_t height, uint16_t bpp);

/* Disable VBE (return to text mode) */
void vbe_disable(void);

/* Get LFB physical address (reads PCI BAR) */
uint32_t vbe_get_lfb_phys(void);

/* Initialize VBE: detect, find LFB, set mode, map into virtual space.
   Returns virtual address of LFB, or NULL on failure. */
void* vbe_init(uint16_t width, uint16_t height, uint16_t bpp);

/* Current display info */
uint16_t vbe_get_width(void);
uint16_t vbe_get_height(void);
uint16_t vbe_get_bpp(void);

#endif /* VBE_H */