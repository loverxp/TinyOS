/* pci.c - List PCI devices (user-mode) */
#include <stdio.h>
#include <stdint.h>
#include <syscall.h>

typedef struct {
    uint16_t vendor;
    uint16_t device;
    uint32_t class_code;
    uint8_t  irq;
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
} pci_dev_info_t;

int main(void) {
    pci_dev_info_t devs[32];
    int bytes = get_system_info(5, devs, sizeof(devs));
    if (bytes <= 0) {
        printf("No PCI devices found.\n");
        return 0;
    }

    int count = bytes / sizeof(pci_dev_info_t);
    printf("PCI devices (%d found):\n", count);
    printf("  Bus:Slot  Vendor  Device  Class     IRQ\n");
    for (int i = 0; i < count; i++) {
        printf("  %02x:%02x     %04x    %04x    %06x    %u\n",
               devs[i].bus, devs[i].slot,
               devs[i].vendor, devs[i].device,
               devs[i].class_code >> 8,
               devs[i].irq);
    }
    return 0;
}
