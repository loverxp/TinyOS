// vbe.c - Bochs VBE (VESA BIOS Extensions) driver for QEMU std VGA
// Program the VBE controller via I/O ports to set high-resolution modes.

#include "../include/vbe.h"
#include "../include/io.h"
#include "../include/pci.h"
#include "../include/paging.h"
#include "../include/stdio.h"

static uint16_t screen_width = 0;
static uint16_t screen_height = 0;
static uint16_t screen_bpp = 0;
static uint32_t lfb_phys = 0;
static void*    lfb_virt = NULL;

// Write a VBE register: index -> port 0x01CE, value -> port 0x01CF
static inline void vbe_write(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

// Read a VBE register
static inline uint16_t vbe_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

int vbe_detect(void) {
    // Write magic version to ID register and read back
    // Bochs VBE returns the version written if supported
    vbe_write(VBE_DISPI_INDEX_ID, 0xB0C2);
    uint16_t id = vbe_read(VBE_DISPI_INDEX_ID);
    // Any version >= 0xB0C0 means Bochs VBE is supported
    return (id >= 0xB0C0) ? 1 : 0;
}

int vbe_set_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    // Disable first
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    // Set resolution and bpp
    vbe_write(VBE_DISPI_INDEX_XRES, width);
    vbe_write(VBE_DISPI_INDEX_YRES, height);
    vbe_write(VBE_DISPI_INDEX_BPP, bpp);

    // Set virtual width = real width (no virtual scrolling)
    vbe_write(VBE_DISPI_INDEX_VIRT_WIDTH, width);
    vbe_write(VBE_DISPI_INDEX_VIRT_HEIGHT, height);

    // Enable with LFB
    vbe_write(VBE_DISPI_INDEX_ENABLE,
        VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM);

    // Verify mode was set
    uint16_t actual_x = vbe_read(VBE_DISPI_INDEX_XRES);
    uint16_t actual_y = vbe_read(VBE_DISPI_INDEX_YRES);
    uint16_t actual_bpp = vbe_read(VBE_DISPI_INDEX_BPP);

    if (actual_x != width || actual_y != height || actual_bpp != bpp) {
        printf("[VBE] Mode set failed: %dx%dx%d -> got %dx%dx%d\n",
            width, height, bpp, actual_x, actual_y, actual_bpp);
        return -1;
    }

    screen_width = width;
    screen_height = height;
    screen_bpp = bpp;
    return 0;
}

void vbe_disable(void) {
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    screen_width = 0;
    screen_height = 0;
    screen_bpp = 0;
}

uint32_t vbe_get_lfb_phys(void) {
    // Try to find LFB address via PCI
    // QEMU std VGA: vendor=0x1234, device=0x1111
    const pci_device_t* vga = pci_find_device(QEMU_VGA_VENDOR, QEMU_VGA_DEVICE);
    if (vga) {
        // BAR0 usually contains the LFB address for VGA controllers
        uint32_t bar0 = vga->bar[0];
        if (bar0 & 1) {
            // I/O space - not what we want. Try BAR2.
            uint32_t bar2 = vga->bar[2];
            if (bar2 & 0xFFFFFFF0) {
                return bar2 & 0xFFFFFFF0;
            }
        } else if (bar0 & 0xFFFFFFF0) {
            return bar0 & 0xFFFFFFF0;
        }
    }

    // Fallback: known QEMU std VGA LFB address
    return 0xE0000000;
}

void* vbe_init(uint16_t width, uint16_t height, uint16_t bpp) {
    printf("[VBE] Detecting Bochs VBE...\n");

    if (!vbe_detect()) {
        printf("[VBE] NOT detected (no Bochs VBE)\n");
        return NULL;
    }
    printf("[VBE] Detected!\n");

    // Get LFB physical address
    lfb_phys = vbe_get_lfb_phys();
    printf("[VBE] LFB physical address: 0x%x\n", lfb_phys);

    // Set mode
    printf("[VBE] Setting mode %dx%dx%d...\n", width, height, bpp);
    if (vbe_set_mode(width, height, bpp) != 0) {
        printf("[VBE] Mode set failed!\n");
        return NULL;
    }
    printf("[VBE] Mode set OK\n");

    // Calculate LFB size and map into virtual memory
    uint32_t fb_size = (uint32_t)width * height * (bpp / 8);
    uint32_t num_pages = (fb_size + 0xFFF) / 0x1000;

    // Map LFB at the same physical address for identity mapping simplicity
    // (since the LFB is above 8MB, we need to allocate page tables)
    uint32_t virt = lfb_phys;  // identity mapping
    printf("[VBE] Mapping %u pages of framebuffer at 0x%x...\n", num_pages, virt);

    for (uint32_t i = 0; i < num_pages; i++) {
        if (paging_map_page(virt + i * 0x1000, lfb_phys + i * 0x1000, 0, 1) != 0) {
            printf("[VBE] Failed to map page %u at 0x%x\n",
                i, virt + i * 0x1000);
            vbe_disable();
            return NULL;
        }
    }

    lfb_virt = (void*)virt;
    printf("[VBE] Framebuffer mapped at 0x%x, size=%uKB\n",
        (uint32_t)lfb_virt, fb_size / 1024);

    return lfb_virt;
}

uint16_t vbe_get_width(void)  { return screen_width; }
uint16_t vbe_get_height(void) { return screen_height; }
uint16_t vbe_get_bpp(void)    { return screen_bpp; }