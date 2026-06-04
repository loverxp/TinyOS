#include "../include/pci.h"
#include "../include/io.h"
#include "../include/stdio.h"
#include "../include/string.h"

static pci_device_t devices[PCI_MAX_DEVICES];
static int device_count = 0;

/* Build PCI config address: bus(8) | device(5) | func(3) | offset(8) | enable(1) */
static uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    return (uint32_t)(1 << 31) |
           ((uint32_t)bus << 16) |
           ((uint32_t)(device & 0x1F) << 11) |
           ((uint32_t)(func & 0x07) << 8) |
           (offset & 0xFC);
}

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    uint32_t addr = pci_config_address(bus, device, func, offset);
    outl(PCI_CONFIG_ADDR, addr);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t addr = pci_config_address(bus, device, func, offset);
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_check_device(uint8_t bus, uint8_t device) {
    uint32_t reg0 = pci_read_config(bus, device, 0, 0x00);
    uint16_t vendor_id = reg0 & 0xFFFF;

    if (vendor_id == 0xFFFF) return;  /* No device */

    /* Check up to 8 functions (if multi-function) */
    uint32_t hdr = pci_read_config(bus, device, 0, 0x0C);
    uint8_t max_func = (hdr & 0x00800000) ? 8 : 1;  /* Multi-function bit */

    for (uint8_t func = 0; func < max_func; func++) {
        uint32_t id = pci_read_config(bus, device, func, 0x00);
        if ((id & 0xFFFF) == 0xFFFF) continue;

        if (device_count >= PCI_MAX_DEVICES) return;

        pci_device_t* dev = &devices[device_count];
        dev->bus = bus;
        dev->device = device;
        dev->func = func;
        dev->vendor_id = id & 0xFFFF;
        dev->device_id = (id >> 16) & 0xFFFF;

        uint32_t class_reg = pci_read_config(bus, device, func, 0x08);
        dev->class_code = (class_reg >> 24) & 0xFF;
        dev->subclass   = (class_reg >> 16) & 0xFF;
        dev->prog_if    = (class_reg >> 8)  & 0xFF;

        uint32_t irq_reg = pci_read_config(bus, device, func, 0x3C);
        dev->irq_line = irq_reg & 0xFF;

        /* Read BARs (Base Address Registers, offsets 0x10-0x24) */
        for (int i = 0; i < 6; i++) {
            dev->bar[i] = pci_read_config(bus, device, func, 0x10 + i * 4);
        }

        device_count++;
    }
}

void pci_scan(void) {
    device_count = 0;
    memset(devices, 0, sizeof(devices));

    /* Scan bus 0, devices 0-31 */
    for (uint8_t dev = 0; dev < 32; dev++) {
        pci_check_device(0, dev);
    }

    serial_printf("[PCI] Found %d devices\n", device_count);
}

const pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor_id &&
            devices[i].device_id == device_id) {
            return &devices[i];
        }
    }
    return NULL;
}

int pci_get_device_count(void) {
    return device_count;
}

const pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= device_count) return NULL;
    return &devices[index];
}

/* Known PCI class codes for display */
static const char* pci_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Comm";
        case 0x08: return "System";
        case 0x09: return "Input";
        case 0x0C: return "Serial Bus";
        default:   return "Unknown";
    }
}

void pci_list_devices(void) {
    printf("PCI devices:\n");
    printf("%-6s %-6s %-3s %-12s %-12s %s\n",
           "Bus", "Dev", "Fn", "Vendor", "Device", "Class");
    printf("----------------------------------------------\n");
    for (int i = 0; i < device_count; i++) {
        const pci_device_t* d = &devices[i];
        printf(" %2u    %2u    %u   0x%04X       0x%04X       %s\n",
               d->bus, d->device, d->func,
               d->vendor_id, d->device_id,
               pci_class_name(d->class_code));
    }
    printf("\n%d device(s) found\n", device_count);
}
